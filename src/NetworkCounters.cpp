#include "NetworkCounters.hpp"

namespace riedel::fabricsperf
{
    std::string readFileContents(std::string_view path)
    {}

    NetworkCounters NetworkCounters::open(std::string_view interfaceAddress)
    {}

    NetworkCounters::NetworkCounters(std::string portCountersDirectory)
        : _dir(std::move(portCountersDirectory))
    {}

    void NetworkCounters::start()
    {}

    void NetworkCounters::stop()
    {}
}
