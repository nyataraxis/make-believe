#include "make_believe/yin_tracker.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace make_believe {

void YinTracker::prepare(const YinSpec& spec) {
    if (spec.sample_rate < 16000 || spec.window_size < 512 ||
        !std::isfinite(spec.minimum_hz) || !std::isfinite(spec.maximum_hz) ||
        spec.minimum_hz < 40.0 || spec.maximum_hz <= spec.minimum_hz ||
        spec.window_size < 2 * spec.sample_rate / spec.minimum_hz ||
        !std::isfinite(spec.threshold) || spec.threshold <= 0.0 || spec.threshold >= 1.0) {
        throw std::invalid_argument("invalid YIN specification");
    }
    spec_ = spec;
    history_.assign(spec.window_size, 0.0f);
    ordered_window_.assign(spec.window_size, 0.0f);
    normalized_difference_.assign(spec.window_size / 2 + 2, 1.0);
    write_index_ = 0;
    available_ = 0;
}

PitchEstimate YinTracker::process(const float* input, std::size_t frames) noexcept {
    if (input == nullptr || history_.empty()) {
        return {};
    }
    for (std::size_t index = 0; index < frames; ++index) {
        history_[write_index_] = input[index];
        write_index_ = (write_index_ + 1) % history_.size();
        available_ = std::min(available_ + 1, history_.size());
    }
    if (available_ < history_.size()) {
        return {};
    }
    const auto size = history_.size();
    double energy = 0.0;
    for (std::size_t index = 0; index < size; ++index) {
        const auto sample = history_[(write_index_ + index) % size];
        ordered_window_[index] = sample;
        energy += double(sample) * sample;
    }
    const double rms = std::sqrt(energy / size);
    PitchEstimate estimate;
    estimate.rms_dbfs = 20.0 * std::log10(std::max(rms, 1e-6));
    if (rms < 1e-4) {
        return estimate;
    }
    const auto minimum_lag = static_cast<std::size_t>(spec_.sample_rate / spec_.maximum_hz);
    const auto maximum_lag = std::min(static_cast<std::size_t>(spec_.sample_rate / spec_.minimum_hz), size / 2);
    double running_sum = 0.0;
    normalized_difference_[0] = 1.0;
    for (std::size_t lag = 1; lag <= maximum_lag; ++lag) {
        double difference = 0.0;
        for (std::size_t index = 0; index < size - lag; ++index) {
            const auto a = ordered_window_[index];
            const auto b = ordered_window_[index + lag];
            const auto delta = double(a) - b;
            difference += delta * delta;
        }
        running_sum += difference;
        normalized_difference_[lag] = running_sum > 0.0 ? difference * lag / running_sum : 1.0;
    }
    std::size_t best_lag = 0;
    for (std::size_t lag = minimum_lag; lag < maximum_lag; ++lag) {
        if (normalized_difference_[lag] < spec_.threshold) {
            while (lag + 1 <= maximum_lag && normalized_difference_[lag + 1] < normalized_difference_[lag]) {
                ++lag;
            }
            best_lag = lag;
            break;
        }
    }
    if (best_lag == 0) {
        return estimate;
    }
    double refined_lag = best_lag;
    if (best_lag > 1 && best_lag < maximum_lag) {
        const double previous = normalized_difference_[best_lag - 1];
        const double current = normalized_difference_[best_lag];
        const double next = normalized_difference_[best_lag + 1];
        const double curvature = previous - 2 * current + next;
        if (curvature > 1e-12) {
            refined_lag += std::clamp(0.5 * (previous - next) / curvature, -0.5, 0.5);
        }
    }
    estimate.f0_hz = spec_.sample_rate / refined_lag;
    estimate.confidence = std::clamp(1.0 - normalized_difference_[best_lag], 0.0, 1.0);
    return estimate;
}

}
