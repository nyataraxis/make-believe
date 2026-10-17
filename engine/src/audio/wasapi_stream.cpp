#include "make_believe/wasapi_stream.hpp"

#include "make_believe/audio_ring.hpp"
#include "make_believe/realtime_morph_processor.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace make_believe {

namespace {
using Microsoft::WRL::ComPtr;

void require_success(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(std::string(operation) + " failed, HRESULT=" +
                                 std::to_string(static_cast<unsigned long>(result)));
    }
}

class ComApartment {
public:
    ComApartment() {
        require_success(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "CoInitializeEx");
    }
    ~ComApartment() {
        CoUninitialize();
    }
};

class AudioClient {
public:
    AudioClient(const std::wstring& endpoint_id, bool capture) {
        try {
            ComPtr<IMMDeviceEnumerator> enumerator;
            require_success(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                             IID_PPV_ARGS(&enumerator)), "CoCreateInstance");
            ComPtr<IMMDevice> device;
            require_success(enumerator->GetDevice(endpoint_id.c_str(), &device), "GetDevice");
            require_success(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                             reinterpret_cast<void**>(client_.GetAddressOf())), "Activate");
            require_success(client_->GetMixFormat(&format_), "GetMixFormat");
            if (format_->nSamplesPerSec != 48000 || format_->wBitsPerSample != 32 ||
                format_->nChannels < 1 || format_->nChannels > 2 || !is_float_format()) {
                throw std::runtime_error("diagnostic stream requires 48 kHz, 1-2 channel, 32-bit float mix format");
            }
            require_success(client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                0, 0, format_, nullptr), "IAudioClient::Initialize");
            require_success(client_->GetBufferSize(&buffer_frames_), "GetBufferSize");
            event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (event_ == nullptr) {
                throw std::runtime_error("CreateEvent failed");
            }
            require_success(client_->SetEventHandle(event_), "SetEventHandle");
            if (capture) {
                require_success(client_->GetService(IID_PPV_ARGS(&capture_client_)), "GetService(capture)");
            } else {
                require_success(client_->GetService(IID_PPV_ARGS(&render_client_)), "GetService(render)");
            }
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~AudioClient() {
        cleanup();
    }

    void start() {
        require_success(client_->Start(), "IAudioClient::Start");
        started_ = true;
    }

    bool wait() const {
        const auto result = WaitForSingleObject(event_, 100);
        if (result == WAIT_FAILED) {
            throw std::runtime_error("WaitForSingleObject failed");
        }
        return result == WAIT_OBJECT_0;
    }

    IAudioClient* client() const { return client_.Get(); }
    IAudioCaptureClient* capture() const { return capture_client_.Get(); }
    IAudioRenderClient* render() const { return render_client_.Get(); }
    UINT32 buffer_frames() const { return buffer_frames_; }
    WORD channels() const { return format_->nChannels; }

private:
    void cleanup() noexcept {
        if (started_) {
            client_->Stop();
            started_ = false;
        }
        if (event_ != nullptr) {
            CloseHandle(event_);
            event_ = nullptr;
        }
        if (format_ != nullptr) {
            CoTaskMemFree(format_);
            format_ = nullptr;
        }
    }
    bool is_float_format() const {
        if (format_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            return true;
        }
        if (format_->wFormatTag != WAVE_FORMAT_EXTENSIBLE ||
            format_->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
            return false;
        }
        const auto* extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format_);
        return IsEqualGUID(extended->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }

    ComPtr<IAudioClient> client_;
    ComPtr<IAudioCaptureClient> capture_client_;
    ComPtr<IAudioRenderClient> render_client_;
    WAVEFORMATEX* format_ = nullptr;
    HANDLE event_ = nullptr;
    UINT32 buffer_frames_ = 0;
    bool started_ = false;
};

struct SharedState {
    SpscAudioRing input{8192};
    SpscAudioRing output{8192};
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> captured{0};
    std::atomic<std::uint64_t> rendered{0};
    std::atomic<std::uint64_t> processed{0};
    std::atomic<std::uint64_t> input_overflow{0};
    std::atomic<std::uint64_t> output_underrun{0};
    std::atomic<std::uint64_t> startup_silence{0};
    std::atomic<std::uint64_t> discontinuities{0};
    std::atomic<std::uint64_t> nonfinite_input{0};
    std::atomic<std::uint64_t> limited_input{0};
    std::atomic<std::uint64_t> nonfinite_output{0};
    std::atomic<std::uint64_t> limited_output{0};
    std::atomic<double> algorithmic_delay_ms{0.0};
    HANDLE input_ready = nullptr;
    std::mutex error_mutex;
    std::condition_variable stopped;
    std::string error;

    void fail(const std::exception& exception) {
        {
            std::lock_guard<std::mutex> guard(error_mutex);
            if (error.empty()) {
                error = exception.what();
            }
        }
        stop.store(true);
        stopped.notify_one();
        SetEvent(input_ready);
    }
};

void capture_loop(SharedState& state, const std::wstring& endpoint_id) {
    try {
        ComApartment apartment;
        AudioClient device(endpoint_id, true);
        std::vector<float> mono(device.buffer_frames());
        device.start();
        while (!state.stop.load()) {
            if (!device.wait()) {
                continue;
            }
            UINT32 packet_frames = 0;
            require_success(device.capture()->GetNextPacketSize(&packet_frames), "GetNextPacketSize");
            while (packet_frames > 0) {
                BYTE* bytes = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                require_success(device.capture()->GetBuffer(&bytes, &frames, &flags, nullptr, nullptr),
                                "IAudioCaptureClient::GetBuffer");
                if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
                    state.discontinuities.fetch_add(1);
                }
                const auto* samples = reinterpret_cast<const float*>(bytes);
                for (UINT32 index = 0; index < frames; ++index) {
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        mono[index] = 0.0f;
                    } else if (device.channels() == 1) {
                        mono[index] = samples[index];
                    } else {
                        mono[index] = 0.5f * (samples[2 * index] + samples[2 * index + 1]);
                    }
                    if (!std::isfinite(mono[index])) {
                        mono[index] = 0.0f;
                        state.nonfinite_input.fetch_add(1);
                    } else if (std::abs(mono[index]) > 1.0f) {
                        mono[index] = std::clamp(mono[index], -1.0f, 1.0f);
                        state.limited_input.fetch_add(1);
                    }
                }
                const auto written = state.input.push(mono.data(), frames);
                state.captured.fetch_add(frames);
                state.input_overflow.fetch_add(frames - written);
                require_success(device.capture()->ReleaseBuffer(frames), "IAudioCaptureClient::ReleaseBuffer");
                SetEvent(state.input_ready);
                require_success(device.capture()->GetNextPacketSize(&packet_frames), "GetNextPacketSize");
            }
        }
    } catch (const std::exception& error) {
        state.fail(error);
    }
}

void dsp_loop(SharedState& state, const DiagnosticStreamSpec& spec) {
    try {
        std::optional<CompiledPreset> preset;
        if (spec.preset_path) {
            preset = load_compiled_preset(*spec.preset_path);
        }
        RealtimeMorphProcessor processor;
        processor.prepare(preset, spec.pitch_ratio, spec.formant_ratio);
        const auto frames = processor.block_size();
        state.algorithmic_delay_ms.store(1000.0 * processor.start_delay_samples() / 48000.0);
        std::vector<float> input(frames);
        std::vector<float> output(frames);
        while (!state.stop.load()) {
            if (state.input.size() < frames || state.output.capacity() - state.output.size() < frames) {
                WaitForSingleObject(state.input_ready, 10);
                continue;
            }
            if (state.input.pop(input.data(), frames) != frames) {
                throw std::runtime_error("DSP input block transfer failed");
            }
            if (!processor.process(input.data(), output.data(), frames)) {
                throw std::runtime_error("DSP processing failed");
            }
            if (state.output.push(output.data(), frames) != frames) {
                throw std::runtime_error("DSP block transfer failed");
            }
            state.processed.fetch_add(1);
        }
        state.nonfinite_output.store(processor.nonfinite_output_samples());
        state.limited_output.store(processor.limited_output_samples());
    } catch (const std::exception& error) {
        state.fail(error);
    }
}

void render_loop(SharedState& state, const std::wstring& endpoint_id) {
    try {
        ComApartment apartment;
        AudioClient device(endpoint_id, false);
        std::vector<float> mono(device.buffer_frames());
        DriftCompensatedReader reader(state.output, 1024);
        device.start();
        while (!state.stop.load()) {
            if (!device.wait()) {
                continue;
            }
            UINT32 padding = 0;
            require_success(device.client()->GetCurrentPadding(&padding), "GetCurrentPadding");
            const UINT32 frames = device.buffer_frames() - padding;
            if (frames == 0) {
                continue;
            }
            BYTE* bytes = nullptr;
            require_success(device.render()->GetBuffer(frames, &bytes), "IAudioRenderClient::GetBuffer");
            const auto available = reader.read(mono.data(), frames);
            auto* samples = reinterpret_cast<float*>(bytes);
            for (UINT32 index = 0; index < frames; ++index) {
                for (WORD channel = 0; channel < device.channels(); ++channel) {
                    samples[index * device.channels() + channel] = mono[index];
                }
            }
            require_success(device.render()->ReleaseBuffer(frames, 0), "IAudioRenderClient::ReleaseBuffer");
            state.rendered.fetch_add(frames);
            if (reader.ever_started()) {
                state.output_underrun.fetch_add(frames - available);
            } else {
                state.startup_silence.fetch_add(frames - available);
            }
            SetEvent(state.input_ready);
        }
    } catch (const std::exception& error) {
        state.fail(error);
    }
}
}

StreamStats run_diagnostic_stream(const DiagnosticStreamSpec& spec) {
    if (spec.input_id.empty() || spec.output_id.empty() || spec.seconds == 0 || spec.seconds > 60 ||
        !std::isfinite(spec.pitch_ratio) || spec.pitch_ratio <= 0.0 ||
        !std::isfinite(spec.formant_ratio) || spec.formant_ratio <= 0.0) {
        throw std::invalid_argument("invalid diagnostic stream specification");
    }
    SharedState state;
    state.input_ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (state.input_ready == nullptr) {
        throw std::runtime_error("CreateEvent failed");
    }
    std::thread capture;
    std::thread dsp;
    std::thread render;
    try {
        capture = std::thread([&] { capture_loop(state, spec.input_id); });
        dsp = std::thread([&] { dsp_loop(state, spec); });
        render = std::thread([&] { render_loop(state, spec.output_id); });
        std::unique_lock<std::mutex> guard(state.error_mutex);
        state.stopped.wait_for(guard, std::chrono::seconds(spec.seconds),
                               [&] { return state.stop.load(); });
    } catch (...) {
        state.stop.store(true);
        SetEvent(state.input_ready);
        if (capture.joinable()) capture.join();
        if (dsp.joinable()) dsp.join();
        if (render.joinable()) render.join();
        CloseHandle(state.input_ready);
        throw;
    }
    state.stop.store(true);
    SetEvent(state.input_ready);
    capture.join();
    dsp.join();
    render.join();
    CloseHandle(state.input_ready);
    if (!state.error.empty()) {
        throw std::runtime_error(state.error);
    }
    return {state.captured.load(), state.rendered.load(), state.processed.load(),
            state.input_overflow.load(), state.output_underrun.load(),
            state.startup_silence.load(), state.discontinuities.load(),
            state.nonfinite_input.load(), state.limited_input.load(),
            state.nonfinite_output.load(), state.limited_output.load(),
            state.algorithmic_delay_ms.load()};
}

}
