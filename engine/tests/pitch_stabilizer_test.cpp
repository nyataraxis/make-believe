#include "make_believe/pitch_stabilizer.hpp"

#include <cmath>
#include <stdexcept>

int main() {
    make_believe::PitchStabilizer stabilizer;
    stabilizer.prepare({70.0, 500.0, 0.65, 10.6667, 30.0, 80.0, 300.0});
    make_believe::StablePitch stable;
    for (int index = 0; index < 10; ++index) {
        stable = stabilizer.process({130.0, 0.95, -15.0});
    }
    if (std::abs(stable.f0_hz - 130.0) > 1.0 || stable.morph_strength < 0.9) {
        throw std::runtime_error("stable pitch was not acquired");
    }
    stable = stabilizer.process({260.0, 0.95, -15.0});
    if (std::abs(stable.f0_hz - 130.0) > 1.0) {
        throw std::runtime_error("octave error was not corrected");
    }
    for (int index = 0; index < 20; ++index) {
        stable = stabilizer.process({0.0, 0.0, -120.0});
    }
    if (stable.morph_strength > 0.1 || std::abs(stable.f0_hz - 130.0) > 1.0) {
        throw std::runtime_error("pitch dropout did not fade smoothly");
    }
    stable = stabilizer.process({220.0, 0.95, -15.0});
    if (stable.morph_strength <= 0.0 || stable.morph_strength >= 0.8 ||
        std::abs(stable.f0_hz - 220.0) > 1.0) {
        throw std::runtime_error("pitch recovery was not gradual");
    }
}
