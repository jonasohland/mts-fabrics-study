#pragma once

#include "ScopedGPUMaxClocks.hpp"

namespace riedel::fabricsperf
{

    class ScopedGPUMaxClocks
    {
    public:
        ScopedGPUMaxClocks(int gpuId);
        ~ScopedGPUMaxClocks();

        // no move, no copy
        ScopedGPUMaxClocks(ScopedGPUMaxClocks const&) = delete;
        ScopedGPUMaxClocks& operator=(ScopedGPUMaxClocks const&) = delete;
        ScopedGPUMaxClocks(ScopedGPUMaxClocks&& other) = delete;
        ScopedGPUMaxClocks& operator=(ScopedGPUMaxClocks&& other) = delete;

    private:
        void set();
        void unset();

        int _gpuId;
    };

}
