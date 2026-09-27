#include "make_believe/compiled_preset.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace make_believe {

namespace {
constexpr std::size_t preset_size = 260;
constexpr std::size_t payload_offset = 80;
constexpr std::size_t payload_size = 176;

std::uint32_t read_u32(const std::array<std::uint8_t, preset_size>& bytes, std::size_t offset) {
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8) |
           (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}

std::uint32_t crc32(const std::array<std::uint8_t, preset_size>& bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t index = 0; index < preset_size - 4; ++index) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
        }
    }
    return ~crc;
}

double read_double(const std::array<std::uint8_t, preset_size>& bytes, std::size_t index) {
    double value;
    std::memcpy(&value, bytes.data() + payload_offset + index * sizeof(double), sizeof(double));
    return value;
}
}

CompiledPreset load_compiled_preset(const std::filesystem::path& path) {
    static_assert(std::endian::native == std::endian::little);
    static_assert(sizeof(double) == 8);
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("cannot open compiled preset");
    }
    std::array<std::uint8_t, preset_size> bytes{};
    stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (stream.gcount() != preset_size || stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("compiled preset has incorrect length");
    }
    const std::array<std::uint8_t, 8> magic{'M', 'B', 'V', 'M', 'P', '1', 0, 0};
    if (!std::equal(magic.begin(), magic.end(), bytes.begin()) ||
        read_u32(bytes, 8) != 1 || read_u32(bytes, 12) != payload_size ||
        read_u32(bytes, preset_size - 4) != crc32(bytes)) {
        throw std::runtime_error("compiled preset header or checksum invalid");
    }
    CompiledPreset preset;
    std::copy_n(bytes.begin() + 16, 32, preset.source_profile_sha256.begin());
    std::copy_n(bytes.begin() + 48, 32, preset.target_profile_sha256.begin());
    for (std::size_t index = 0; index < 7; ++index) {
        preset.pitch.source_knots_log2_hz[index] = read_double(bytes, index);
        preset.pitch.target_knots_log2_hz[index] = read_double(bytes, index + 7);
    }
    preset.pitch.strength = read_double(bytes, 14);
    preset.pitch.minimum_output_hz = read_double(bytes, 15);
    preset.pitch.maximum_output_hz = read_double(bytes, 16);
    preset.pitch.maximum_shift_semitones = read_double(bytes, 17);
    preset.pitch.minimum_extrapolation_slope = read_double(bytes, 18);
    preset.pitch.maximum_extrapolation_slope = read_double(bytes, 19);
    preset.formant_ratio = read_double(bytes, 20);
    preset.formant_strength = read_double(bytes, 21);
    PitchMapper validator;
    validator.configure(preset.pitch);
    if (!std::isfinite(preset.formant_ratio) || preset.formant_ratio < 0.5 ||
        preset.formant_ratio > 1.5 || !std::isfinite(preset.formant_strength) ||
        preset.formant_strength < 0.0 || preset.formant_strength > 1.0) {
        throw std::runtime_error("compiled preset formant controls invalid");
    }
    return preset;
}

}
