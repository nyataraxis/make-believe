#pragma once

#include <string>
#include <vector>

namespace make_believe {

enum class AudioDirection {
    capture,
    render,
};

struct AudioEndpoint {
    std::wstring id;
    std::wstring name;
    AudioDirection direction;
};

struct AudioEndpointFormat {
    std::size_t sample_rate;
    std::size_t channels;
    std::size_t bits_per_sample;
    bool floating_point;
    double default_period_ms;
};

std::vector<AudioEndpoint> list_audio_endpoints();
AudioEndpointFormat query_audio_endpoint_format(const std::wstring& id);

}
