#include "Nvml.hpp"
#include <cstdint>
#include <nvml.h>

namespace riedel::fabricsperf
{

    Nvml::Nvml()
    {
        nvmlCall(nvmlInit, "initialize library");
    }

    double Nvml::samplePcieThroughputRx(std::uint64_t deviceId)
    {
        nvmlDevice_t device;
        nvmlCall(nvmlDeviceGetHandleByIndex, "get device handle", deviceId, &device);

        unsigned int throughput = 0;
        nvmlCall(nvmlDeviceGetPcieThroughput,
            "query pcie throughput",
            device,
            NVML_PCIE_UTIL_RX_BYTES,
            &throughput);

        // Value is in KB/s, let's normalize it to B/s
        return static_cast<double>(throughput) * 1000.0;
    }

    double Nvml::samplePcieThroughputTx(std::uint64_t deviceId)
    {
        nvmlDevice_t device;
        nvmlCall(nvmlDeviceGetHandleByIndex,
            "get device handle",
            deviceId,
            &device); // Value is in KB/s, let's normalize it to B/s

        unsigned int throughput = 0;
        nvmlCall(nvmlDeviceGetPcieThroughput,
            "query pcie throughput",
            device,
            NVML_PCIE_UTIL_TX_BYTES,
            &throughput);

        // Value in KB/s let's normalize it to B/s
        return static_cast<double>(throughput) * 1000.0;
    }

}
