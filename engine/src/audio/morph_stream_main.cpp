#include "make_believe/wasapi_stream.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const bool fixed = argc == 7 && std::string(argv[3]) == "--fixed";
    const bool preset = argc == 6 && std::string(argv[3]) == "--preset";
    if (!fixed && !preset) {
        std::cerr << "usage: morph_stream <capture-id> <render-id> --fixed <pitch-ratio> <formant-ratio> <seconds>\n"
                     "       morph_stream <capture-id> <render-id> --preset <preset.vmp> <seconds>\n";
        return 2;
    }
    try {
        make_believe::DiagnosticStreamSpec spec;
        spec.input_id = std::wstring(argv[1], argv[1] + std::string(argv[1]).size());
        spec.output_id = std::wstring(argv[2], argv[2] + std::string(argv[2]).size());
        if (fixed) {
            spec.pitch_ratio = std::stod(argv[4]);
            spec.formant_ratio = std::stod(argv[5]);
            spec.seconds = static_cast<unsigned>(std::stoul(argv[6]));
        } else {
            spec.preset_path = argv[4];
            spec.seconds = static_cast<unsigned>(std::stoul(argv[5]));
        }
        const auto stats = make_believe::run_diagnostic_stream(spec);
        std::cout << "captured_frames=" << stats.captured_frames
                  << " rendered_frames=" << stats.rendered_frames
                  << " processed_blocks=" << stats.processed_blocks
                  << " input_overflow_frames=" << stats.input_overflow_frames
                  << " output_underrun_frames=" << stats.output_underrun_frames
                  << " startup_silence_frames=" << stats.startup_silence_frames
                  << " capture_discontinuities=" << stats.capture_discontinuities
                  << " nonfinite_input_samples=" << stats.nonfinite_input_samples
                  << " limited_input_samples=" << stats.limited_input_samples
                  << " nonfinite_output_samples=" << stats.nonfinite_output_samples
                  << " limited_output_samples=" << stats.limited_output_samples
                  << " algorithmic_delay_ms=" << stats.algorithmic_delay_ms << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
