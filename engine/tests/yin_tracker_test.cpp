#include "make_believe/yin_tracker.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    make_believe::YinTracker tracker;
    tracker.prepare({48000, 2048, 70.0, 500.0, 0.12});
    std::vector<float> block(512);
    for (const double frequency : {120.0, 220.0}) {
        for (std::size_t iteration = 0; iteration < 20; ++iteration) {
            for (std::size_t index = 0; index < block.size(); ++index) {
                const auto sample = iteration * block.size() + index;
                block[index] = 0.2f * std::sin(2.0 * 3.141592653589793 * frequency * sample / 48000.0);
            }
            const auto estimate = tracker.process(block.data(), block.size());
            if (iteration == 19 && (std::abs(estimate.f0_hz - frequency) > 1.0 || estimate.confidence < 0.8)) {
                throw std::runtime_error("YIN failed known tone");
            }
        }
    }
    std::fill(block.begin(), block.end(), 0.0f);
    for (std::size_t iteration = 0; iteration < 5; ++iteration) {
        const auto estimate = tracker.process(block.data(), block.size());
        if (iteration == 4 && estimate.f0_hz != 0.0) {
            throw std::runtime_error("YIN detected a pitch in silence");
        }
    }
    make_believe::YinTracker noise_tracker;
    noise_tracker.prepare({48000, 2048, 70.0, 500.0, 0.35});
    std::uint32_t state = 0x12345678u;
    for (std::size_t iteration = 0; iteration < 30; ++iteration) {
        for (float& sample : block) {
            state = state * 1664525u + 1013904223u;
            sample = 0.2f * (static_cast<float>(state >> 8) / 8388608.0f - 1.0f);
        }
        const auto estimate = noise_tracker.process(block.data(), block.size());
        if (estimate.f0_hz > 0.0 && estimate.confidence >= 0.65) {
            throw std::runtime_error("YIN accepted broadband noise as voiced speech");
        }
    }
    std::cout << "YIN tone and silence checks passed\n";
}
