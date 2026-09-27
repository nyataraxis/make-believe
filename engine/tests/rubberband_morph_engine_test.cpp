#include "make_believe/rubberband_morph_engine.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("morph engine check failed");
    }
}

int main() {
    make_believe::RubberBandMorphEngine engine;
    engine.prepare({48000, 512, 1.7, 1.12});
    const auto block_size = engine.block_size();
    require(block_size > 0 && block_size <= 512);
    std::vector<float> input(block_size);
    std::vector<float> output(block_size);
    for (std::size_t block = 0; block < 100; ++block) {
        for (std::size_t index = 0; index < block_size; ++index) {
            const auto sample = block * block_size + index;
            input[index] = 0.2f * std::sin(2.0 * 3.141592653589793 * 130.0 * sample / 48000.0);
        }
        const make_believe::RuntimeControls controls{1.7, 1.12};
        require(engine.process(input.data(), output.data(), block_size, controls));
        for (const float value : output) {
            require(std::isfinite(value));
        }
    }
    require(!engine.process(input.data(), output.data(), block_size - 1, {1.0, 1.0}));
    make_believe::RubberBandMorphEngine medium;
    medium.prepare({48000, 512, 1.7, 1.12, make_believe::AnalysisWindow::medium_window});
    require(medium.block_size() == block_size);
    require(medium.start_delay_samples() > engine.start_delay_samples());
    require(medium.process(input.data(), output.data(), block_size, {1.7, 1.12}));
    std::cout << "block_size=" << block_size << " start_delay_samples="
              << engine.start_delay_samples() << " start_delay_ms="
              << 1000.0 * engine.start_delay_samples() / 48000.0 << '\n';
    for (const double ratio : {1.0, 1.5, 2.0}) {
        make_believe::RubberBandMorphEngine candidate;
        candidate.prepare({48000, 512, ratio, 1.12});
        std::cout << "pitch_ratio=" << ratio << " start_delay_ms="
                  << 1000.0 * candidate.start_delay_samples() / 48000.0 << '\n';
    }
}
