#pragma once

#include <string>

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
        std::string test;
        std::string node;
        std::string service;
        std::string output;
        std::string gpu;
        std::string domain;
        std::string flow;

        Mode mode() const;
        int listenPort() const;
        std::string listenHost() const;
        std::string flowConfig() const;
    };

}
