#include "make_believe/compiled_preset.hpp"

#include <cmath>
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: preset_inspector <preset.vmp>\n";
        return 2;
    }
    try {
        const auto preset = make_believe::load_compiled_preset(argv[1]);
        std::cout << "source_median_hz=" << std::exp2(preset.pitch.source_knots_log2_hz[3])
                  << " target_median_hz=" << std::exp2(preset.pitch.target_knots_log2_hz[3])
                  << " formant_ratio=" << preset.formant_ratio << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
