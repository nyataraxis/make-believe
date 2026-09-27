#include "make_believe/audio_devices.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#include <windows.h>

namespace {
std::string utf8(const std::wstring& value) {
    const auto length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        throw std::runtime_error("WideCharToMultiByte failed");
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), length, nullptr, nullptr);
    result.pop_back();
    return result;
}
}

int main(int argc, char** argv) {
    try {
        const bool show_formats = argc == 2 && std::string(argv[1]) == "--formats";
        if (argc > 1 && !show_formats) {
            std::cerr << "usage: audio_devices [--formats]\n";
            return 2;
        }
        for (const auto& endpoint : make_believe::list_audio_endpoints()) {
            std::cout << (endpoint.direction == make_believe::AudioDirection::capture ? "capture" : "render")
                      << '\t' << utf8(endpoint.name) << '\t' << utf8(endpoint.id);
            if (show_formats) {
                const auto format = make_believe::query_audio_endpoint_format(endpoint.id);
                std::cout << '\t' << format.sample_rate << " Hz / " << format.channels
                          << " ch / " << format.bits_per_sample << " bit / "
                          << (format.floating_point ? "float" : "integer")
                          << " / period " << format.default_period_ms << " ms";
            }
            std::cout << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
