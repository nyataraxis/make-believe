#include "make_believe/audio_ring.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <thread>

int main() {
    make_believe::SpscAudioRing ring(64);
    std::array<float, 80> input{};
    std::array<float, 80> output{};
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<float>(index);
    }
    if (ring.push(input.data(), input.size()) != 64 || ring.size() != 64 ||
        ring.pop(output.data(), 40) != 40 || ring.push(input.data() + 40, 40) != 40 ||
        ring.pop(output.data(), 64) != 64 || ring.size() != 0) {
        throw std::runtime_error("ring wraparound check failed");
    }

    make_believe::SpscAudioRing concurrent(127);
    std::atomic<bool> valid{true};
    std::thread producer([&] {
        for (int value = 0; value < 100000; ++value) {
            const float sample = static_cast<float>(value);
            while (concurrent.push(&sample, 1) == 0) {
                std::this_thread::yield();
            }
        }
    });
    std::thread consumer([&] {
        for (int expected = 0; expected < 100000; ++expected) {
            float sample = -1.0f;
            while (concurrent.pop(&sample, 1) == 0) {
                std::this_thread::yield();
            }
            if (sample != static_cast<float>(expected)) {
                valid.store(false);
            }
        }
    });
    producer.join();
    consumer.join();
    if (!valid.load() || concurrent.size() != 0) {
        throw std::runtime_error("ring concurrent order check failed");
    }

    make_believe::SpscAudioRing drifting(4096);
    std::array<float, 1024> priming{};
    std::fill(priming.begin(), priming.end(), 0.25f);
    drifting.push(priming.data(), priming.size());
    make_believe::DriftCompensatedReader reader(drifting, 1024);
    std::array<float, 481> block{};
    std::fill(block.begin(), block.end(), 0.25f);
    std::array<float, 480> rendered{};
    for (int iteration = 0; iteration < 4000; ++iteration) {
        const std::size_t input_frames = iteration % 20 == 0 ? 481 : 480;
        if (drifting.push(block.data(), input_frames) != input_frames ||
            reader.read(rendered.data(), rendered.size()) != rendered.size() ||
            drifting.size() < 400 || drifting.size() > 1800) {
            throw std::runtime_error("drift compensation lost bounded occupancy");
        }
    }
    if (reader.ratio() <= 1.0 || std::abs(rendered[240] - 0.25f) > 0.001f) {
        throw std::runtime_error("drift compensation did not increase consumption smoothly");
    }
}
