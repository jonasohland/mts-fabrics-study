#pragma once

#include <optional>

namespace riedel::fabricsperf
{

    class ScopedGPUMaxClocks
    {
    public:
        ScopedGPUMaxClocks() = default;

        ScopedGPUMaxClocks(int gpuId);
        ~ScopedGPUMaxClocks();

        // no move, no copy
        ScopedGPUMaxClocks(ScopedGPUMaxClocks const&) = delete;
        ScopedGPUMaxClocks& operator=(ScopedGPUMaxClocks const&) = delete;
        ScopedGPUMaxClocks(ScopedGPUMaxClocks&& other);
        ScopedGPUMaxClocks& operator=(ScopedGPUMaxClocks&& other);

        void setId(int gpuId);

    private:
        void set();
        void unset();

        std::optional<int> _gpuId = std::nullopt;
    };

}
