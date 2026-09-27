#include "make_believe/realtime_morph_processor.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace make_believe {

void RealtimeMorphProcessor::prepare(const std::optional<CompiledPreset>& preset,
                                     double fixed_pitch_ratio, double fixed_formant_ratio,
                                     AnalysisWindow window) {
    if (!std::isfinite(fixed_pitch_ratio) || fixed_pitch_ratio <= 0.0 ||
        !std::isfinite(fixed_formant_ratio) || fixed_formant_ratio <= 0.0) {
        throw std::invalid_argument("invalid fixed morph ratios");
    }
    has_preset_ = preset.has_value();
    fixed_pitch_ratio_ = fixed_pitch_ratio;
    target_formant_ratio_ = fixed_formant_ratio;
    double initial_pitch_ratio = fixed_pitch_ratio;
    if (preset) {
        mapper_.configure(preset->pitch);
        const auto source_median = std::exp2(preset->pitch.source_knots_log2_hz[3]);
        const auto target_median = std::exp2(preset->pitch.target_knots_log2_hz[3]);
        initial_pitch_ratio = target_median / source_median;
        target_formant_ratio_ = 1.0 + preset->formant_strength * (preset->formant_ratio - 1.0);
        const auto minimum_hz = std::max(70.0, std::exp2(preset->pitch.source_knots_log2_hz[0]) * 0.7);
        const auto maximum_hz = std::min(500.0, std::exp2(preset->pitch.source_knots_log2_hz[6]) * 1.5);
        tracker_.prepare({48000, 2048, minimum_hz, maximum_hz, 0.35});
        stabilizer_.prepare({minimum_hz, maximum_hz, 0.65, 1000.0 * 512 / 48000.0,
                             30.0, 80.0, 300.0});
    }
    engine_.prepare({48000, 512, initial_pitch_ratio, target_formant_ratio_, window});
    nonfinite_output_samples_ = 0;
    limited_output_samples_ = 0;
    last_pitch_ratio_ = fixed_pitch_ratio_;
    last_morph_strength_ = 0.0;
    last_estimated_f0_hz_ = 0.0;
    last_pitch_confidence_ = 0.0;
}

bool RealtimeMorphProcessor::process(const float* input, float* output,
                                     std::size_t frames) noexcept {
    if (input == nullptr || output == nullptr || frames != engine_.block_size()) {
        return false;
    }
    double pitch_ratio = fixed_pitch_ratio_;
    double formant_ratio = target_formant_ratio_;
    last_morph_strength_ = 0.0;
    if (has_preset_) {
        const auto estimate = tracker_.process(input, frames);
        last_estimated_f0_hz_ = estimate.f0_hz;
        last_pitch_confidence_ = estimate.confidence;
        const auto stable = stabilizer_.process(estimate);
        last_morph_strength_ = stable.morph_strength;
        pitch_ratio = 1.0;
        if (stable.f0_hz > 0.0) {
            const auto mapped_hz = mapper_.map_hz(stable.f0_hz);
            pitch_ratio = std::exp2(stable.morph_strength *
                                    std::log2(mapped_hz / stable.f0_hz));
        }
        formant_ratio = 1.0 + stable.morph_strength * (target_formant_ratio_ - 1.0);
    }
    last_pitch_ratio_ = pitch_ratio;
    if (!engine_.process(input, output, frames, {pitch_ratio, formant_ratio})) {
        return false;
    }
    for (std::size_t index = 0; index < frames; ++index) {
        auto& sample = output[index];
        if (!std::isfinite(sample)) {
            sample = 0.0f;
            ++nonfinite_output_samples_;
        } else if (std::abs(sample) > 0.89125f) {
            sample = std::clamp(sample, -0.89125f, 0.89125f);
            ++limited_output_samples_;
        }
    }
    return true;
}

std::size_t RealtimeMorphProcessor::block_size() const noexcept {
    return engine_.block_size();
}

std::size_t RealtimeMorphProcessor::start_delay_samples() const noexcept {
    return engine_.start_delay_samples();
}

std::uint64_t RealtimeMorphProcessor::nonfinite_output_samples() const noexcept {
    return nonfinite_output_samples_;
}

std::uint64_t RealtimeMorphProcessor::limited_output_samples() const noexcept {
    return limited_output_samples_;
}

double RealtimeMorphProcessor::last_pitch_ratio() const noexcept {
    return last_pitch_ratio_;
}

double RealtimeMorphProcessor::last_morph_strength() const noexcept {
    return last_morph_strength_;
}

double RealtimeMorphProcessor::last_estimated_f0_hz() const noexcept {
    return last_estimated_f0_hz_;
}

double RealtimeMorphProcessor::last_pitch_confidence() const noexcept {
    return last_pitch_confidence_;
}

}
