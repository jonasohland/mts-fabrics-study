#pragma once

namespace riedel::fabricsperf
{
    enum class PollMode
    {
        WAIT,
        SPIN
    };

    enum class TransferMode
    {
        Reflect,
        OneWay
    };
}
