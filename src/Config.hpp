#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace riedel::fabricsperf
{
    enum class Mode
    {
        RUNNER,
        REFLECTOR
    };

    struct Config
    {
        bool runner;
        bool reflector;
        std::string listen;
        std::string connect;
        std::vector<std::string> run;
        std::string targetEndpoint;
        std::string initiatorEndpoint;
        std::string output;
        std::vector<std::uint64_t> gpu;
        std::string domain;
        std::string flow;
        std::size_t iterations;

        Mode mode() const;
        int listenPort() const;
        std::string listenHost() const;
        std::string flowConfig() const;

        std::string targetEndpointNode() const;
        std::string targetEndpointService() const;
        std::string initiatorEndpointNode() const;
        std::string initiatorEndpointService() const;
    };

}
