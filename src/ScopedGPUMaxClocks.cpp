#include "ScopedGPUMaxClocks.hpp"
#include <exception>
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

    ScopedGPUMaxClocks::ScopedGPUMaxClocks(int gpuId)
        : _gpuId(gpuId)
    {
        set();
    }

    ScopedGPUMaxClocks::~ScopedGPUMaxClocks()
    {
        try
        {
            unset();
        }
        catch (std::exception& ex)
        {
            MXL_ERROR("failed to reset gpu clocks to defaults: {}", ex.what());
        }
    }

    void ScopedGPUMaxClocks::set()
    {
        nvmlDevice_t device;
        uint32_t maxGraphicsClock;
        uint32_t maxMemoryClock;

        nvmlCall(nvmlInit, "initialize library");
        nvmlCall(nvmlDeviceGetHandleByIndex, "get device handle", _gpuId, &device);

        // get max clock info
        nvmlCall(nvmlDeviceGetMaxClockInfo,
            "get max graphics clock",
            device,
            NVML_CLOCK_GRAPHICS,
            &maxGraphicsClock);
        nvmlCall(nvmlDeviceGetMaxClockInfo,
            "get max graphics clock",
            device,
            NVML_CLOCK_MEM,
            &maxMemoryClock);

        // lock clocks to max
        nvmlCall(nvmlDeviceSetGpuLockedClocks,
            "lock max graphics clock",
            device,
            maxGraphicsClock,
            maxGraphicsClock);
        nvmlCall(nvmlDeviceSetMemoryLockedClocks,
            "lock max memory clock",
            device,
            maxMemoryClock,
            maxMemoryClock);

        MXL_INFO("gpu clocks locked at: [gpu: {}, memory: {}]", maxGraphicsClock, maxMemoryClock);
    }

    void ScopedGPUMaxClocks::unset()
    {
        nvmlDevice_t device;
        nvmlCall(nvmlInit, "initialize library");
        nvmlCall(nvmlDeviceGetHandleByIndex, "get device handle", _gpuId, &device);
        nvmlCall(nvmlDeviceResetGpuLockedClocks, "reset locked gpu clocks", device);
        nvmlCall(nvmlDeviceResetMemoryLockedClocks, "reset locked memory clocks", device);

        MXL_INFO("gpu clocks reset");
    }
}
