#pragma once

#include <memory>

#include "make_believe/morph_engine.hpp"

namespace RubberBand {
class RubberBandLiveShifter;
}

namespace make_believe {

class RubberBandMorphEngine final : public IMorphEngine {
public:
    RubberBandMorphEngine();
    ~RubberBandMorphEngine() override;
    void prepare(const EngineSpec& spec) override;
    bool process(const float* input, float* output, std::size_t frames,
                 const RuntimeControls& controls) noexcept override;
    std::size_t block_size() const noexcept override;
    std::size_t start_delay_samples() const noexcept override;

private:
    std::unique_ptr<RubberBand::RubberBandLiveShifter> shifter_;
    std::size_t block_size_ = 0;
    std::size_t start_delay_samples_ = 0;
};

}
