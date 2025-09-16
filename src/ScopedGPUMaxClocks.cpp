#include "ScopedGPUMaxClocks.hpp"
#include <exception>
#include <stdexcept>
#include <nvml.h>
#include <fmt/format.h>
#include "internal/Logging.hpp"
#include "Nvml.hpp"

namespace riedel::fabricsperf
{

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

    ScopedGPUMaxClocks::ScopedGPUMaxClocks(ScopedGPUMaxClocks&& other)
        : _gpuId{other._gpuId}
    {
        other._gpuId.reset();
    }

    ScopedGPUMaxClocks& ScopedGPUMaxClocks::operator=(ScopedGPUMaxClocks&& other)
    {
        unset();

        _gpuId = other._gpuId;
        other._gpuId.reset();

        return *this;
    }

    void ScopedGPUMaxClocks::setId(int gpuId)
    {
        if (_gpuId.has_value())
        {
            throw std::runtime_error("GPU Id was already set, can't set it twice!");
        }

        _gpuId = gpuId;

        set();
    }

    void ScopedGPUMaxClocks::set()
    {
        if (_gpuId)
        {
            nvmlDevice_t device;
            uint32_t maxGraphicsClock;
            uint32_t maxMemoryClock;

            nvmlCall(nvmlInit, "initialize library");
            nvmlCall(nvmlDeviceGetHandleByIndex, "get device handle", *_gpuId, &device);

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

            MXL_INFO(
                "gpu clocks locked at: [gpu: {}, memory: {}]", maxGraphicsClock, maxMemoryClock);
        }
    }

    void ScopedGPUMaxClocks::unset()
    {
        if (_gpuId)
        {
            nvmlDevice_t device;
            nvmlCall(nvmlInit, "initialize library");
            nvmlCall(nvmlDeviceGetHandleByIndex, "get device handle", *_gpuId, &device);
            nvmlCall(nvmlDeviceResetGpuLockedClocks, "reset locked gpu clocks", device);
            nvmlCall(nvmlDeviceResetMemoryLockedClocks, "reset locked memory clocks", device);

            MXL_INFO("gpu clocks reset");
        }
    }
}
