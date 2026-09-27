#pragma once

#include <cstddef>
#include <vector>

namespace make_believe {

struct PitchEstimate {
    double f0_hz = 0.0;
    double confidence = 0.0;
    double rms_dbfs = -120.0;
};

struct YinSpec {
    std::size_t sample_rate = 48000;
    std::size_t window_size = 2048;
    double minimum_hz = 70.0;
    double maximum_hz = 500.0;
    double threshold = 0.12;
};

class YinTracker {
public:
    void prepare(const YinSpec& spec);
    PitchEstimate process(const float* input, std::size_t frames) noexcept;

private:
    YinSpec spec_{};
    std::vector<float> history_;
    std::vector<float> ordered_window_;
    std::vector<double> normalized_difference_;
    std::size_t write_index_ = 0;
    std::size_t available_ = 0;
};

}
