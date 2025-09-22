#pragma once

#include <string>
#include <string_view>

namespace riedel::fabricsperf
{
    class NetworkCounters
    {
    public:
        static NetworkCounters open(std::string_view interfaceAddress);

        void start();
        void stop();

    private:
        NetworkCounters(std::string portCountersDirectory);

        std::string _dir;
    };
}
