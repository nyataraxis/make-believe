#pragma once

#include "make_believe/compiled_preset.hpp"
#include "make_believe/pitch_mapper.hpp"
#include "make_believe/pitch_stabilizer.hpp"
#include "make_believe/rubberband_morph_engine.hpp"
#include "make_believe/yin_tracker.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace make_believe {

class RealtimeMorphProcessor {
public:
    void prepare(const std::optional<CompiledPreset>& preset, double fixed_pitch_ratio,
                 double fixed_formant_ratio,
                 AnalysisWindow window = AnalysisWindow::short_window);
    bool process(const float* input, float* output, std::size_t frames) noexcept;
    std::size_t block_size() const noexcept;
    std::size_t start_delay_samples() const noexcept;
    std::uint64_t nonfinite_output_samples() const noexcept;
    std::uint64_t limited_output_samples() const noexcept;
    double last_pitch_ratio() const noexcept;
    double last_morph_strength() const noexcept;
    double last_estimated_f0_hz() const noexcept;
    double last_pitch_confidence() const noexcept;

private:
    RubberBandMorphEngine engine_;
    PitchMapper mapper_;
    YinTracker tracker_;
    PitchStabilizer stabilizer_;
    double fixed_pitch_ratio_ = 1.0;
    double target_formant_ratio_ = 1.0;
    bool has_preset_ = false;
    std::uint64_t nonfinite_output_samples_ = 0;
    std::uint64_t limited_output_samples_ = 0;
    double last_pitch_ratio_ = 1.0;
    double last_morph_strength_ = 0.0;
    double last_estimated_f0_hz_ = 0.0;
    double last_pitch_confidence_ = 0.0;
};

}
