#include "make_believe/pitch_stabilizer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace make_believe {

void PitchStabilizer::prepare(const PitchStabilizerSpec& spec) {
    if (!std::isfinite(spec.minimum_hz) || !std::isfinite(spec.maximum_hz) ||
        spec.minimum_hz <= 0.0 || spec.maximum_hz <= spec.minimum_hz ||
        !std::isfinite(spec.confidence_threshold) || spec.confidence_threshold < 0.0 ||
        spec.confidence_threshold > 1.0 || !std::isfinite(spec.frame_duration_ms) ||
        spec.frame_duration_ms <= 0.0 || !std::isfinite(spec.attack_ms) ||
        spec.attack_ms <= 0.0 || !std::isfinite(spec.release_ms) ||
        spec.release_ms <= 0.0 || !std::isfinite(spec.maximum_step_cents) ||
        spec.maximum_step_cents <= 0.0) {
        throw std::invalid_argument("invalid pitch stabilizer specification");
    }
    spec_ = spec;
    recent_log2_hz_.fill(0.0);
    recent_count_ = 0;
    next_index_ = 0;
    smoothed_log2_hz_ = 0.0;
    strength_ = 0.0;
    has_pitch_ = false;
    ready_ = true;
}

StablePitch PitchStabilizer::process(const PitchEstimate& estimate) noexcept {
    if (!ready_) {
        return {};
    }
    double candidate = estimate.f0_hz;
    bool accepted = std::isfinite(candidate) && candidate >= spec_.minimum_hz &&
                    candidate <= spec_.maximum_hz && std::isfinite(estimate.confidence) &&
                    estimate.confidence >= spec_.confidence_threshold;
    if (accepted && strength_ < 0.1) {
        has_pitch_ = false;
        recent_count_ = 0;
        next_index_ = 0;
    }
    if (accepted && has_pitch_) {
        const auto previous_hz = std::exp2(smoothed_log2_hz_);
        for (const double alternative : {candidate * 0.5, candidate * 2.0}) {
            if (alternative >= spec_.minimum_hz && alternative <= spec_.maximum_hz &&
                std::abs(std::log2(alternative / previous_hz)) + 0.1 <
                    std::abs(std::log2(candidate / previous_hz))) {
                candidate = alternative;
            }
        }
        const auto step_cents = std::abs(1200.0 * std::log2(candidate / previous_hz));
        if (step_cents > spec_.maximum_step_cents) {
            accepted = false;
        }
    }
    if (accepted) {
        const auto log_pitch = std::log2(candidate);
        recent_log2_hz_[next_index_] = log_pitch;
        next_index_ = (next_index_ + 1) % recent_log2_hz_.size();
        recent_count_ = std::min(recent_count_ + 1, recent_log2_hz_.size());
        auto sorted = recent_log2_hz_;
        std::sort(sorted.begin(), sorted.begin() + recent_count_);
        const auto median = sorted[recent_count_ / 2];
        if (!has_pitch_) {
            smoothed_log2_hz_ = median;
            has_pitch_ = true;
        } else {
            const auto alpha = 1.0 - std::exp(-spec_.frame_duration_ms / spec_.attack_ms);
            smoothed_log2_hz_ += alpha * (median - smoothed_log2_hz_);
        }
    }
    const auto duration = accepted ? spec_.attack_ms : spec_.release_ms;
    const auto alpha = 1.0 - std::exp(-spec_.frame_duration_ms / duration);
    strength_ += alpha * ((accepted ? 1.0 : 0.0) - strength_);
    return {has_pitch_ ? std::exp2(smoothed_log2_hz_) : 0.0, strength_};
}

}
