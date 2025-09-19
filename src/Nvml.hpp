#pragma once

#include <cstdint>
#include <nvml.h>
#include <fmt/format.h>
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{
    template<typename F, typename S, typename... Args>
    void nvmlCall(F fun, S msg, Args... args)
    {
        if (auto status = fun(std::forward<Args>(args)...); status != NVML_SUCCESS)
        {
            if (status == NVML_ERROR_NOT_SUPPORTED)
            {
                MXL_WARN("{} is not supported", msg);
                return;
            }

            throw std::runtime_error(
                fmt::format("nvml: {}: {}", std::forward<S>(msg), nvmlErrorString(status)));
        }
    }

    class Nvml
    {
    public:
        Nvml();

        double samplePcieThroughputTx(std::uint64_t deviceId);
        double samplePcieThroughputRx(std::uint64_t deviceId);

    private:
    };

}
