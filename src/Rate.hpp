#pragma once

#include <chrono>

namespace riedel::fabricsperf
{
    class RateTimer
    {
    public:
        RateTimer(int64_t num, int64_t den);

        bool waitUntilNextFrame();

    private:
        std::chrono::steady_clock::time_point _lastFrame;
        std::chrono::nanoseconds _timeBetweenFrames;
    };
}
