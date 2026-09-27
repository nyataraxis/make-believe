#include "make_believe/audio_devices.hpp"

#include <stdexcept>

#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <wrl/client.h>

namespace make_believe {

namespace {
using Microsoft::WRL::ComPtr;

void require_success(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(operation);
    }
}

void append_endpoints(IMMDeviceEnumerator* enumerator, EDataFlow flow,
                      AudioDirection direction, std::vector<AudioEndpoint>& output) {
    ComPtr<IMMDeviceCollection> collection;
    require_success(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection),
                    "EnumAudioEndpoints failed");
    UINT count = 0;
    require_success(collection->GetCount(&count), "GetCount failed");
    for (UINT index = 0; index < count; ++index) {
        ComPtr<IMMDevice> device;
        require_success(collection->Item(index, &device), "IMMDeviceCollection::Item failed");
        LPWSTR raw_id = nullptr;
        require_success(device->GetId(&raw_id), "IMMDevice::GetId failed");
        std::wstring id(raw_id);
        CoTaskMemFree(raw_id);
        ComPtr<IPropertyStore> properties;
        require_success(device->OpenPropertyStore(STGM_READ, &properties), "OpenPropertyStore failed");
        PROPVARIANT name_value;
        PropVariantInit(&name_value);
        require_success(properties->GetValue(PKEY_Device_FriendlyName, &name_value),
                        "GetValue(PKEY_Device_FriendlyName) failed");
        const std::wstring name = name_value.vt == VT_LPWSTR ? name_value.pwszVal : L"Unknown";
        PropVariantClear(&name_value);
        output.push_back({id, name, direction});
    }
}
}

std::vector<AudioEndpoint> list_audio_endpoints() {
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    require_success(initialized, "CoInitializeEx failed");
    try {
        std::vector<AudioEndpoint> endpoints;
        {
            ComPtr<IMMDeviceEnumerator> enumerator;
            require_success(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                             IID_PPV_ARGS(&enumerator)),
                            "CoCreateInstance(MMDeviceEnumerator) failed");
            append_endpoints(enumerator.Get(), eCapture, AudioDirection::capture, endpoints);
            append_endpoints(enumerator.Get(), eRender, AudioDirection::render, endpoints);
        }
        CoUninitialize();
        return endpoints;
    } catch (...) {
        CoUninitialize();
        throw;
    }
}

AudioEndpointFormat query_audio_endpoint_format(const std::wstring& id) {
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    require_success(initialized, "CoInitializeEx failed");
    try {
        AudioEndpointFormat result{};
        {
            ComPtr<IMMDeviceEnumerator> enumerator;
            require_success(CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                             IID_PPV_ARGS(&enumerator)),
                            "CoCreateInstance(MMDeviceEnumerator) failed");
            ComPtr<IMMDevice> device;
            require_success(enumerator->GetDevice(id.c_str(), &device), "GetDevice failed");
            ComPtr<IAudioClient> client;
            require_success(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                             reinterpret_cast<void**>(client.GetAddressOf())),
                            "IAudioClient activation failed");
            WAVEFORMATEX* format = nullptr;
            require_success(client->GetMixFormat(&format), "GetMixFormat failed");
            REFERENCE_TIME default_period = 0;
            REFERENCE_TIME minimum_period = 0;
            const auto period_result = client->GetDevicePeriod(&default_period, &minimum_period);
            if (FAILED(period_result)) {
                CoTaskMemFree(format);
                require_success(period_result, "GetDevicePeriod failed");
            }
            bool floating_point = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
            if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                format->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
                const auto* extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
                floating_point = IsEqualGUID(extended->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            }
            result = {format->nSamplesPerSec, format->nChannels, format->wBitsPerSample,
                      floating_point, default_period / 10000.0};
            CoTaskMemFree(format);
        }
        CoUninitialize();
        return result;
    } catch (...) {
        CoUninitialize();
        throw;
    }
}

}
