#include "make_believe/audio_ring.hpp"

#include <algorithm>
#include <stdexcept>

namespace make_believe {

SpscAudioRing::SpscAudioRing(std::size_t capacity) : data_(capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("audio ring capacity must be positive");
    }
}

std::size_t SpscAudioRing::push(const float* input, std::size_t frames) noexcept {
    if (input == nullptr) {
        return 0;
    }
    const auto write = write_position_.load(std::memory_order_relaxed);
    const auto read = read_position_.load(std::memory_order_acquire);
    const auto count = std::min<std::size_t>(frames, data_.size() - (write - read));
    const auto offset = static_cast<std::size_t>(write % data_.size());
    const auto first = std::min(count, data_.size() - offset);
    std::copy_n(input, first, data_.data() + offset);
    std::copy_n(input + first, count - first, data_.data());
    write_position_.store(write + count, std::memory_order_release);
    return count;
}

std::size_t SpscAudioRing::pop(float* output, std::size_t frames) noexcept {
    if (output == nullptr) {
        return 0;
    }
    const auto read = read_position_.load(std::memory_order_relaxed);
    const auto write = write_position_.load(std::memory_order_acquire);
    const auto count = std::min<std::size_t>(frames, write - read);
    const auto offset = static_cast<std::size_t>(read % data_.size());
    const auto first = std::min(count, data_.size() - offset);
    std::copy_n(data_.data() + offset, first, output);
    std::copy_n(data_.data(), count - first, output + first);
    read_position_.store(read + count, std::memory_order_release);
    return count;
}

std::size_t SpscAudioRing::size() const noexcept {
    const auto read = read_position_.load(std::memory_order_acquire);
    const auto write = write_position_.load(std::memory_order_acquire);
    return std::min<std::size_t>(data_.size(), write - read);
}

std::size_t SpscAudioRing::capacity() const noexcept {
    return data_.size();
}

DriftCompensatedReader::DriftCompensatedReader(SpscAudioRing& ring,
                                               std::size_t target_fill_frames)
    : ring_(ring), target_fill_frames_(target_fill_frames) {
    if (target_fill_frames < 2 || target_fill_frames >= ring.capacity()) {
        throw std::invalid_argument("target ring fill must be between 2 and capacity - 1");
    }
}

std::size_t DriftCompensatedReader::read(float* output, std::size_t frames) noexcept {
    if (output == nullptr) {
        return 0;
    }
    if (!started_) {
        if (ring_.size() < target_fill_frames_ || ring_.pop(&previous_, 1) == 0 ||
            ring_.pop(&next_, 1) == 0) {
            std::fill_n(output, frames, 0.0f);
            return 0;
        }
        started_ = true;
        ever_started_ = true;
        phase_ = 0.0;
    }
    const auto occupancy = ring_.size();
    const auto error = static_cast<double>(occupancy) - target_fill_frames_;
    const auto target_ratio = std::clamp(1.0 + 0.002 * error / target_fill_frames_,
                                         0.999, 1.001);
    ratio_ += 0.05 * (target_ratio - ratio_);
    std::size_t produced = 0;
    for (; produced < frames; ++produced) {
        output[produced] = static_cast<float>(previous_ + phase_ * (next_ - previous_));
        phase_ += ratio_;
        while (phase_ >= 1.0) {
            previous_ = next_;
            phase_ -= 1.0;
            if (ring_.pop(&next_, 1) == 0) {
                started_ = false;
                ++produced;
                std::fill(output + produced, output + frames, 0.0f);
                return produced;
            }
        }
    }
    return produced;
}

bool DriftCompensatedReader::ever_started() const noexcept {
    return ever_started_;
}

double DriftCompensatedReader::ratio() const noexcept {
    return ratio_;
}

}
