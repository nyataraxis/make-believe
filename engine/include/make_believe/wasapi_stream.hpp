#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace make_believe {

struct DiagnosticStreamSpec {
    std::wstring input_id;
    std::wstring output_id;
    double pitch_ratio = 1.7;
    double formant_ratio = 1.12;
    std::optional<std::filesystem::path> preset_path;
    unsigned seconds = 5;
};

struct StreamStats {
    std::uint64_t captured_frames = 0;
    std::uint64_t rendered_frames = 0;
    std::uint64_t processed_blocks = 0;
    std::uint64_t input_overflow_frames = 0;
    std::uint64_t output_underrun_frames = 0;
    std::uint64_t startup_silence_frames = 0;
    std::uint64_t capture_discontinuities = 0;
    std::uint64_t nonfinite_input_samples = 0;
    std::uint64_t limited_input_samples = 0;
    std::uint64_t nonfinite_output_samples = 0;
    std::uint64_t limited_output_samples = 0;
    double algorithmic_delay_ms = 0.0;
};

StreamStats run_diagnostic_stream(const DiagnosticStreamSpec& spec);

}
