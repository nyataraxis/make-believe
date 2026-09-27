#include "make_believe/compiled_preset.hpp"
#include "make_believe/realtime_morph_processor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 4 && !(argc == 6 && std::string(argv[4]) == "--window" &&
                       (std::string(argv[5]) == "short" || std::string(argv[5]) == "medium"))) {
        std::cerr << "usage: morph_raw <48khz-mono-float32-input> <preset.vmp> <output.f32> [--window short|medium]\n";
        return 2;
    }
    bool output_created = false;
    const std::filesystem::path output_path = argv[3];
    try {
        const std::filesystem::path input_path = argv[1];
        if (std::filesystem::exists(output_path)) {
            throw std::invalid_argument("output already exists");
        }
        const auto byte_count = std::filesystem::file_size(input_path);
        if (byte_count == 0 || byte_count % sizeof(float) != 0 ||
            byte_count > 60ull * 48000 * sizeof(float)) {
            throw std::invalid_argument("input must contain up to 60 seconds of raw 48 kHz float32 mono audio");
        }
        const auto preset = make_believe::load_compiled_preset(argv[2]);
        make_believe::RealtimeMorphProcessor processor;
        const auto window = argc == 6 && std::string(argv[5]) == "medium" ?
            make_believe::AnalysisWindow::medium_window :
            make_believe::AnalysisWindow::short_window;
        processor.prepare(preset, 1.0, 1.0, window);
        const auto block_size = processor.block_size();
        const auto input_frames = byte_count / sizeof(float);
        const auto input_blocks = (input_frames + block_size - 1) / block_size;
        const auto flush_blocks = (processor.start_delay_samples() + block_size - 1) / block_size + 1;
        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("cannot open raw audio input");
        }
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot open raw audio output");
        }
        output_created = true;
        std::vector<float> block(block_size, 0.0f);
        std::vector<float> morphed(block_size);
        double total_pitch_ratio = 0.0;
        std::size_t active_blocks = 0;
        std::size_t estimated_pitch_blocks = 0;
        std::size_t confident_pitch_blocks = 0;
        for (std::size_t index = 0; index < input_blocks + flush_blocks; ++index) {
            std::fill(block.begin(), block.end(), 0.0f);
            if (index < input_blocks) {
                const auto remaining = input_frames - index * block_size;
                const auto count = std::min<std::uintmax_t>(block_size, remaining);
                input.read(reinterpret_cast<char*>(block.data()), count * sizeof(float));
                if (input.gcount() != static_cast<std::streamsize>(count * sizeof(float))) {
                    throw std::runtime_error("raw audio input ended early");
                }
                for (std::size_t sample = 0; sample < count; ++sample) {
                    if (!std::isfinite(block[sample]) || std::abs(block[sample]) > 1.0f) {
                        throw std::invalid_argument("raw input contains nonfinite or out-of-range samples");
                    }
                }
            }
            if (!processor.process(block.data(), morphed.data(), block_size)) {
                throw std::runtime_error("native processor rejected a block");
            }
            if (index < input_blocks) {
                total_pitch_ratio += processor.last_pitch_ratio();
                active_blocks += processor.last_morph_strength() > 0.5;
                estimated_pitch_blocks += processor.last_estimated_f0_hz() > 0.0;
                confident_pitch_blocks += processor.last_pitch_confidence() >= 0.65;
            }
            output.write(reinterpret_cast<const char*>(morphed.data()),
                         block_size * sizeof(float));
            if (!output) {
                throw std::runtime_error("raw audio output failed");
            }
        }
        output.close();
        if (!output) {
            throw std::runtime_error("raw audio output could not be finalized");
        }
        output_created = false;
        std::cout << "input_frames=" << input_frames
                  << " output_frames=" << (input_blocks + flush_blocks) * block_size
                  << " start_delay_samples=" << processor.start_delay_samples()
                  << " nonfinite_output_samples=" << processor.nonfinite_output_samples()
                  << " limited_output_samples=" << processor.limited_output_samples() << '\n';
        std::cout << "mean_pitch_ratio=" << total_pitch_ratio / input_blocks
                  << " active_morph_blocks=" << active_blocks
                  << " estimated_pitch_blocks=" << estimated_pitch_blocks
                  << " confident_pitch_blocks=" << confident_pitch_blocks << '\n';
    } catch (const std::exception& error) {
        if (output_created) {
            std::error_code ignored;
            std::filesystem::remove(output_path, ignored);
        }
        std::cerr << error.what() << '\n';
        return 1;
    }
}
