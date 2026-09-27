#pragma once

#include <array>

namespace make_believe {

struct PitchMapConfig {
    std::array<double, 7> source_knots_log2_hz{};
    std::array<double, 7> target_knots_log2_hz{};
    double strength = 1.0;
    double minimum_output_hz = 70.0;
    double maximum_output_hz = 1000.0;
    double maximum_shift_semitones = 18.0;
    double minimum_extrapolation_slope = 0.25;
    double maximum_extrapolation_slope = 2.0;
};

class PitchMapper {
public:
    void configure(const PitchMapConfig& config);
    double map_hz(double input_hz) const noexcept;

private:
    PitchMapConfig config_{};
    bool ready_ = false;
};

}
