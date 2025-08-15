#pragma once

#include <httplib.h>
#include "Config.hpp"
#include "Executor.hpp"

namespace riedel::fabricsperf
{
    class Runner : public Executor::Inner
    {
    public:
        Runner(Config const& config);

        void stop() override;
        void run() override;

    private:
        Config const& _config;
        httplib::Server _srv;
    };
}
