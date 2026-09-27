#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace make_believe {

class SpscAudioRing {
public:
    explicit SpscAudioRing(std::size_t capacity);
    std::size_t push(const float* input, std::size_t frames) noexcept;
    std::size_t pop(float* output, std::size_t frames) noexcept;
    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;

private:
    std::vector<float> data_;
    alignas(64) std::atomic<std::uint64_t> write_position_{0};
    alignas(64) std::atomic<std::uint64_t> read_position_{0};
};

class DriftCompensatedReader {
public:
    DriftCompensatedReader(SpscAudioRing& ring, std::size_t target_fill_frames);
    std::size_t read(float* output, std::size_t frames) noexcept;
    bool ever_started() const noexcept;
    double ratio() const noexcept;

private:
    SpscAudioRing& ring_;
    std::size_t target_fill_frames_;
    float previous_ = 0.0f;
    float next_ = 0.0f;
    double phase_ = 0.0;
    double ratio_ = 1.0;
    bool started_ = false;
    bool ever_started_ = false;
};

}
