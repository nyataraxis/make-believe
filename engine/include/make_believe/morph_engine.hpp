#pragma once

#include <cstddef>

namespace make_believe {

enum class AnalysisWindow { short_window, medium_window };

struct EngineSpec {
    std::size_t sample_rate = 48000;
    std::size_t maximum_block_size = 512;
    double initial_pitch_ratio = 1.0;
    double initial_formant_ratio = 1.0;
    AnalysisWindow analysis_window = AnalysisWindow::short_window;
};

struct RuntimeControls {
    double pitch_ratio = 1.0;
    double formant_ratio = 1.0;
};

class IMorphEngine {
public:
    virtual ~IMorphEngine() = default;

    virtual void prepare(const EngineSpec& spec) = 0;
    virtual bool process(const float* input, float* output, std::size_t frames,
                         const RuntimeControls& controls) noexcept = 0;
    virtual std::size_t block_size() const noexcept = 0;
    virtual std::size_t start_delay_samples() const noexcept = 0;
};

}
