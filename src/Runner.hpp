#pragma once

#include <httplib.h>
#include "Config.hpp"

namespace riedel::fabricsperf
{
    class Runner
    {
    public:
        Runner(Config const& config);

        void run();

    private:
        Config const& _config;
        httplib::Server _srv;
    };
}
