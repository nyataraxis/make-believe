#pragma once

#include <array>

#include "make_believe/yin_tracker.hpp"

namespace make_believe {

struct PitchStabilizerSpec {
    double minimum_hz = 70.0;
    double maximum_hz = 500.0;
    double confidence_threshold = 0.65;
    double frame_duration_ms = 10.6667;
    double attack_ms = 30.0;
    double release_ms = 80.0;
    double maximum_step_cents = 300.0;
};

struct StablePitch {
    double f0_hz = 0.0;
    double morph_strength = 0.0;
};

class PitchStabilizer {
public:
    void prepare(const PitchStabilizerSpec& spec);
    StablePitch process(const PitchEstimate& estimate) noexcept;

private:
    PitchStabilizerSpec spec_{};
    std::array<double, 3> recent_log2_hz_{};
    std::size_t recent_count_ = 0;
    std::size_t next_index_ = 0;
    double smoothed_log2_hz_ = 0.0;
    double strength_ = 0.0;
    bool ready_ = false;
    bool has_pitch_ = false;
};

}
