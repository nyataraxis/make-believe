#include "make_believe/rubberband_morph_engine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <rubberband/RubberBandLiveShifter.h>

namespace make_believe {

namespace {
class SilentLogger final : public RubberBand::RubberBandLiveShifter::Logger {
public:
    void log(const char*) override {}
    void log(const char*, double) override {}
    void log(const char*, double, double) override {}
};
}

RubberBandMorphEngine::RubberBandMorphEngine() = default;
RubberBandMorphEngine::~RubberBandMorphEngine() = default;

void RubberBandMorphEngine::prepare(const EngineSpec& spec) {
    if (spec.sample_rate < 16000 || spec.maximum_block_size == 0 ||
        !std::isfinite(spec.initial_pitch_ratio) || spec.initial_pitch_ratio <= 0.0 ||
        spec.initial_pitch_ratio > 4.0 || !std::isfinite(spec.initial_formant_ratio) ||
        spec.initial_formant_ratio < 0.5 || spec.initial_formant_ratio > 1.5) {
        throw std::invalid_argument("invalid engine specification");
    }
    const auto window_option = spec.analysis_window == AnalysisWindow::medium_window ?
        RubberBand::RubberBandLiveShifter::OptionWindowMedium :
        RubberBand::RubberBandLiveShifter::OptionWindowShort;
    auto shifter = std::make_unique<RubberBand::RubberBandLiveShifter>(
        spec.sample_rate, 1, std::make_shared<SilentLogger>(),
        RubberBand::RubberBandLiveShifter::OptionFormantPreserved | window_option);
    const auto block_size = shifter->getBlockSize();
    if (block_size > spec.maximum_block_size) {
        throw std::invalid_argument("Rubber Band block size exceeds maximum_block_size");
    }
    shifter->setPitchScale(spec.initial_pitch_ratio);
    shifter->setFormantScale(spec.initial_formant_ratio / spec.initial_pitch_ratio);
    const auto start_delay = shifter->getStartDelay();
    shifter_ = std::move(shifter);
    block_size_ = block_size;
    start_delay_samples_ = start_delay;
}

bool RubberBandMorphEngine::process(const float* input, float* output, std::size_t frames,
                                    const RuntimeControls& controls) noexcept {
    if (input == nullptr || output == nullptr || input == output || shifter_ == nullptr ||
        frames != block_size_ || !std::isfinite(controls.pitch_ratio) ||
        !std::isfinite(controls.formant_ratio) || controls.pitch_ratio <= 0.0 ||
        controls.pitch_ratio > 4.0 || controls.formant_ratio < 0.5 ||
        controls.formant_ratio > 1.5) {
        if (output != nullptr) {
            std::fill_n(output, std::min(frames, block_size_), 0.0f);
        }
        return false;
    }
    shifter_->setPitchScale(controls.pitch_ratio);
    shifter_->setFormantScale(controls.formant_ratio / controls.pitch_ratio);
    const float* input_channels[] = {input};
    float* output_channels[] = {output};
    shifter_->shift(input_channels, output_channels);
    return true;
}

std::size_t RubberBandMorphEngine::block_size() const noexcept {
    return block_size_;
}

std::size_t RubberBandMorphEngine::start_delay_samples() const noexcept {
    return start_delay_samples_;
}

}
