#include "make_believe/pitch_mapper.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace make_believe {

void PitchMapper::configure(const PitchMapConfig& config) {
    if (!std::isfinite(config.strength) || config.strength < 0.0 || config.strength > 1.0 ||
        !std::isfinite(config.minimum_output_hz) || config.minimum_output_hz <= 0.0 ||
        !std::isfinite(config.maximum_output_hz) || config.maximum_output_hz <= config.minimum_output_hz ||
        !std::isfinite(config.maximum_shift_semitones) || config.maximum_shift_semitones <= 0.0 ||
        !std::isfinite(config.minimum_extrapolation_slope) || config.minimum_extrapolation_slope <= 0.0 ||
        !std::isfinite(config.maximum_extrapolation_slope) || config.maximum_extrapolation_slope < config.minimum_extrapolation_slope) {
        throw std::invalid_argument("invalid pitch map limits");
    }
    for (std::size_t index = 0; index < config.source_knots_log2_hz.size(); ++index) {
        if (!std::isfinite(config.source_knots_log2_hz[index]) ||
            !std::isfinite(config.target_knots_log2_hz[index]) ||
            (index > 0 && (config.source_knots_log2_hz[index] <= config.source_knots_log2_hz[index - 1] ||
                           config.target_knots_log2_hz[index] <= config.target_knots_log2_hz[index - 1]))) {
            throw std::invalid_argument("pitch knots must be finite and strictly increasing");
        }
    }
    config_ = config;
    ready_ = true;
}

double PitchMapper::map_hz(double input_hz) const noexcept {
    if (!ready_ || !std::isfinite(input_hz) || input_hz <= 0.0) {
        return 0.0;
    }
    if (config_.strength == 0.0) {
        return input_hz;
    }
    const auto x = std::log2(input_hz);
    const auto& source = config_.source_knots_log2_hz;
    const auto& target = config_.target_knots_log2_hz;
    double mapped;
    if (x < source.front()) {
        const auto slope = std::clamp((target[1] - target[0]) / (source[1] - source[0]),
                                      config_.minimum_extrapolation_slope, config_.maximum_extrapolation_slope);
        mapped = target.front() + slope * (x - source.front());
    } else if (x > source.back()) {
        const auto last = source.size() - 1;
        const auto slope = std::clamp((target[last] - target[last - 1]) / (source[last] - source[last - 1]),
                                      config_.minimum_extrapolation_slope, config_.maximum_extrapolation_slope);
        mapped = target.back() + slope * (x - source.back());
    } else {
        const auto upper = std::upper_bound(source.begin(), source.end(), x);
        const auto index = std::min<std::size_t>(std::distance(source.begin(), upper), source.size() - 1);
        if (index == 0) {
            mapped = target.front();
        } else {
            const auto fraction = (x - source[index - 1]) / (source[index] - source[index - 1]);
            mapped = target[index - 1] + fraction * (target[index] - target[index - 1]);
        }
    }
    const auto shift = std::clamp(mapped - x, -config_.maximum_shift_semitones / 12.0,
                                  config_.maximum_shift_semitones / 12.0);
    return std::clamp(std::exp2(x + config_.strength * shift), config_.minimum_output_hz,
                      config_.maximum_output_hz);
}

}
