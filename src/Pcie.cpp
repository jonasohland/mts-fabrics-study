#include "Pcie.hpp"
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <optional>
#include <thread>
#include <utility>
#include "internal/Logging.hpp"
#include "Nvml.hpp"

namespace riedel::fabricsperf
{

    // `nvmlDeviceGetPcieThroughput` is querying a byte counter over a 20ms interval and thus
    // the value returned is the PCIe throughput over that interval
    // We will spawn a new native thread which will collect samples by calling
    // nvmlDeviceGetPcieThroughput every 20ms.
    NvmlPcieRecorder::NvmlPcieRecorder(std::uint64_t deviceId)
        : _nvml(Nvml())
        , _deviceId(deviceId)
        , _terminate(false)
    {}

    NvmlPcieRecorder::~NvmlPcieRecorder()
    {
        stop();
    }

    NvmlPcieRecorder::NvmlPcieRecorder(NvmlPcieRecorder&& other) noexcept
        : _nvml(std::move(other._nvml))
        , _deviceId(other._deviceId)
        , _throughputs(std::move(other._throughputs))
        , _handle(std::move(other._handle))
        , _terminate(false)
    {
        _handle = std::nullopt;
    }

    NvmlPcieRecorder& NvmlPcieRecorder::operator=(NvmlPcieRecorder&& other) noexcept
    {
        stop();

        _nvml = std::move(other._nvml);
        _deviceId = other._deviceId;
        _throughputs = std::move(other._throughputs);
        _handle = std::move(other._handle);
        _handle = std::nullopt;

        _terminate = false;

        return *this;
    }

    void NvmlPcieRecorder::start()
    {
        if (!_handle)
        {
            _throughputs.clear();
            _handle = std::thread([&]() { this->record(); });
        }
        else
        {
            MXL_WARN("Attempted to start a NvmlPcieRecorder that was already started, ignoring the "
                     "request.");
        }
    }

    void NvmlPcieRecorder::stop()
    {
        if (_handle)
        {
            _terminate = true;
            _handle->join();
            _handle = std::nullopt;
        }
    }

    std::vector<std::pair<std::string, std::string>>
    NvmlPcieRecorder::exportCounters() const noexcept
    {
        std::vector<std::pair<std::string, std::string>> out;

        std::ranges::transform(_throughputs.begin(),
            _throughputs.end(),
            std::back_inserter(out),
            [](PcieThroughput const& entry) -> std::pair<std::string, std::string>
            { return {std::to_string(entry.tx), std::to_string(entry.rx)}; });

        return out;
    }

    void NvmlPcieRecorder::record()
    {
        auto nextSampleTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);

        while (!_terminate)
        {
            _throughputs.push_back(
                {_nvml.samplePcieThroughputTx(_deviceId), _nvml.samplePcieThroughputRx(_deviceId)});

            std::this_thread::sleep_until(nextSampleTime);
            nextSampleTime = nextSampleTime + std::chrono::milliseconds(20);
        }
    }
}
