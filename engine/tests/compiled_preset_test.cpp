#include "make_believe/compiled_preset.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        throw std::runtime_error("compiled preset fixture path required");
    }
    const auto preset = make_believe::load_compiled_preset(argv[1]);
    if (preset.formant_ratio <= 1.0 || preset.pitch.target_knots_log2_hz[3] <=
                                         preset.pitch.source_knots_log2_hz[3]) {
        throw std::runtime_error("compiled preset decoded incorrect values");
    }
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
    if (bytes.size() != 260) {
        throw std::runtime_error("compiled preset fixture has incorrect size");
    }
    auto damaged = bytes;
    damaged[90] ^= 1;
    const auto path = std::filesystem::current_path() / "corrupt-preset.vmp";
    {
        std::ofstream output(path, std::ios::binary);
        output.write(damaged.data(), damaged.size());
    }
    bool rejected = false;
    try {
        make_believe::load_compiled_preset(path);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    std::filesystem::remove(path);
    if (!rejected) {
        throw std::runtime_error("compiled preset checksum mismatch accepted");
    }
}
