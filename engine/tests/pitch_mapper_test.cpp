#include "make_believe/pitch_mapper.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    make_believe::PitchMapConfig config;
    const double source_hz[] = {90, 100, 115, 130, 150, 175, 195};
    const double target_hz[] = {170, 185, 205, 225, 250, 280, 310};
    for (std::size_t index = 0; index < 7; ++index) {
        config.source_knots_log2_hz[index] = std::log2(source_hz[index]);
        config.target_knots_log2_hz[index] = std::log2(target_hz[index]);
    }
    make_believe::PitchMapper mapper;
    mapper.configure(config);
    if (mapper.map_hz(0.0) != 0.0 || std::abs(mapper.map_hz(130.0) - 225.0) > 0.001) {
        throw std::runtime_error("pitch mapping failed at a knot");
    }
    double previous = 0.0;
    for (double input = 60.0; input <= 400.0; input += 0.25) {
        const auto output = mapper.map_hz(input);
        if (output < previous || !std::isfinite(output)) {
            throw std::runtime_error("pitch mapping is not monotonic and finite");
        }
        previous = output;
    }
    config.target_knots_log2_hz[3] = config.target_knots_log2_hz[2];
    bool rejected = false;
    try {
        mapper.configure(config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected) {
        throw std::runtime_error("non-monotonic knots were accepted");
    }
    config.target_knots_log2_hz[3] = std::log2(target_hz[3]);
    config.strength = 0.0;
    mapper.configure(config);
    if (mapper.map_hz(35.0) != 35.0) {
        throw std::runtime_error("zero-strength mapping changed input pitch");
    }
    std::cout << "pitch mapping checks passed\n";
}
