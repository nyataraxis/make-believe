#pragma once

#include <array>
#include <cstdint>
#include <filesystem>

#include "make_believe/pitch_mapper.hpp"

namespace make_believe {

struct CompiledPreset {
    PitchMapConfig pitch;
    double formant_ratio = 1.0;
    double formant_strength = 1.0;
    std::array<std::uint8_t, 32> source_profile_sha256{};
    std::array<std::uint8_t, 32> target_profile_sha256{};
};

CompiledPreset load_compiled_preset(const std::filesystem::path& path);

}
