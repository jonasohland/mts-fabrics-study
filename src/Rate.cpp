#include "Rate.hpp"
#include <thread>

namespace riedel::fabricsperf
{
    RateTimer::RateTimer(int64_t num, int64_t den)
        : _lastFrame(std::chrono::steady_clock::now())
        , _timeBetweenFrames(static_cast<uint64_t>(
              static_cast<double>(std::nano::den) /
              (static_cast<double>(num) / static_cast<double>(den))))
    {}

    bool RateTimer::waitUntilNextFrame()
    {
        auto now = std::chrono::steady_clock::now();
        auto timeToWait = _timeBetweenFrames -
                          std::chrono::duration_cast<std::chrono::nanoseconds>(now - _lastFrame);
        if (timeToWait.count() <= 0)
        {
            _lastFrame = now;
            return false;
        }

        std::this_thread::sleep_for(timeToWait);
        _lastFrame = std::chrono::steady_clock::now();

        return true;
    }
}
