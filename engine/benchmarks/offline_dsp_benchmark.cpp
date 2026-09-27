#include "make_believe/realtime_morph_processor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const auto blocks = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 3000;
    if (blocks < 100) {
        throw std::invalid_argument("at least 100 blocks required");
    }
    make_believe::CompiledPreset preset;
    const double source_hz[] = {90, 100, 115, 130, 150, 175, 195};
    const double target_hz[] = {170, 185, 205, 225, 250, 280, 310};
    for (std::size_t index = 0; index < 7; ++index) {
        preset.pitch.source_knots_log2_hz[index] = std::log2(source_hz[index]);
        preset.pitch.target_knots_log2_hz[index] = std::log2(target_hz[index]);
    }
    preset.formant_ratio = 1.12;
    make_believe::RealtimeMorphProcessor processor;
    const auto window = argc > 2 && std::string(argv[2]) == "medium" ?
        make_believe::AnalysisWindow::medium_window :
        make_believe::AnalysisWindow::short_window;
    processor.prepare(preset, 1.0, 1.0, window);
    const auto frames = processor.block_size();
    std::vector<float> input(frames);
    std::vector<float> output(frames);
    std::vector<double> timings;
    timings.reserve(blocks);
    for (std::size_t block = 0; block < blocks; ++block) {
        for (std::size_t index = 0; index < frames; ++index) {
            const auto sample = block * frames + index;
            input[index] = 0.2f * std::sin(2.0 * 3.141592653589793 * 130.0 * sample / 48000.0);
        }
        const auto start = std::chrono::steady_clock::now();
        if (!processor.process(input.data(), output.data(), frames)) {
            throw std::runtime_error("morph engine rejected benchmark block");
        }
        const auto end = std::chrono::steady_clock::now();
        if (block >= 20) {
            timings.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }
    }
    std::sort(timings.begin(), timings.end());
    const auto p99_index = static_cast<std::size_t>(0.99 * (timings.size() - 1));
    std::cout << "blocks=" << blocks << " frames_per_block=" << frames
              << " window=" << (window == make_believe::AnalysisWindow::medium_window ? "medium" : "short")
              << " start_delay_ms=" << 1000.0 * processor.start_delay_samples() / 48000.0
              << " block_duration_ms=" << 1000.0 * frames / 48000.0
              << " p50_dsp_ms=" << timings[timings.size() / 2]
              << " p99_dsp_ms=" << timings[p99_index]
              << " max_dsp_ms=" << timings.back() << '\n';
}
