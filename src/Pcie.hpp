#pragma once

#include <cstdint>
#include <optional>
#include <thread>
#include <vector>
#include "Nvml.hpp"

namespace riedel::fabricsperf
{

    struct PcieThroughput
    {
        // Directions are seen from the pov of the device
        double tx; // For a GPU that would be device to host
        double rx; // For a GPU that would be host to device
    };

    class NvmlPcieRecorder

    {
    public:
        NvmlPcieRecorder(std::uint64_t deviceId);
        ~NvmlPcieRecorder();

        NvmlPcieRecorder(NvmlPcieRecorder const&) = delete;
        NvmlPcieRecorder& operator=(NvmlPcieRecorder const&) = delete;

        NvmlPcieRecorder(NvmlPcieRecorder&&) noexcept;
        NvmlPcieRecorder& operator=(NvmlPcieRecorder&&) noexcept;
        void start();
        void stop();

        std::vector<std::pair<std::string, std::string>> exportCounters() const noexcept;

    private:
        void record();

    private:
        Nvml _nvml;
        std::uint64_t _deviceId;

        // Data
        std::vector<PcieThroughput> _throughputs;

        /// Bakcground thread stuff
        std::optional<std::thread> _handle;
        std::atomic<bool> _terminate;
    };
}
