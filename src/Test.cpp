#include "Test.hpp"
#include <algorithm>
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{

    TestContext::TestContext(Config const& config)
        : _timers(config.iterations)
        , _timeRecords(config.iterations)
        , _isRunner(config.mode() == Mode::RUNNER)
        , _config(config)
    {
        _perfRecorder.addEvent("context_switches",
            PERF_TYPE_SOFTWARE,
            PERF_COUNT_SW_CONTEXT_SWITCHES,
            PerfRecorder::Filter::None);

        _perfRecorder.addEvent("cycles_user",
            PERF_TYPE_HARDWARE,
            PERF_COUNT_HW_CPU_CYCLES,
            PerfRecorder::Filter::User);
        _perfRecorder.addEvent("cycles_kernel",
            PERF_TYPE_HARDWARE,
            PERF_COUNT_HW_CPU_CYCLES,
            PerfRecorder::Filter::Kernel);

        _perfRecorder.addEvent("instructions_user",
            PERF_TYPE_HARDWARE,
            PERF_COUNT_HW_INSTRUCTIONS,
            PerfRecorder::Filter::User);
        _perfRecorder.addEvent("instructions_kernel",
            PERF_TYPE_HARDWARE,
            PERF_COUNT_HW_INSTRUCTIONS,
            PerfRecorder::Filter::Kernel);

        _perfRecorder.addEvent("cpu_clock_user",
            PERF_TYPE_SOFTWARE,
            PERF_COUNT_SW_CPU_CLOCK,
            PerfRecorder::Filter::User);
        _perfRecorder.addEvent("cpu_clock_kernel",
            PERF_TYPE_SOFTWARE,
            PERF_COUNT_SW_CPU_CLOCK,
            PerfRecorder::Filter::Kernel);

        _perfRecorder.addEvent("task_clock_user",
            PERF_TYPE_SOFTWARE,
            PERF_COUNT_SW_TASK_CLOCK,
            PerfRecorder::Filter::User);
        _perfRecorder.addEvent("task_clock_kernel",
            PERF_TYPE_SOFTWARE,
            PERF_COUNT_SW_TASK_CLOCK,
            PerfRecorder::Filter::Kernel);

        _perfRecorder.start();
    }

    TestContext::~TestContext()
    {
        if (_flows)
        {
            try
            {
                _flows->destroy();
            }
            catch (std::exception& ex)
            {
                MXL_ERROR("failed to destroy flow setup: {}", ex.what());
            }
        }
    }

    FlowSetup& TestContext::flows()
    {
        if (!_flows)
        {
            throw std::runtime_error("flows not set up");
        }

        return *_flows;
    }

    void TestContext::timerStart(uint64_t index)
    {
        auto now = std::chrono::system_clock::now();
        auto tindex = getTimerIndex(index);

        _timers[tindex].start = now;
        _timeRecords[tindex] = now;
    }

    void TestContext::timerStop(uint64_t index)
    {
        auto now = std::chrono::system_clock::now();
        auto tindex = getTimerIndex(index);

        _timers[tindex].stop = now;
        _timers[tindex].isValid = true;
    }

    void TestContext::recordCurrentTime(uint64_t index)
    {
        auto now = std::chrono::system_clock::now();
        auto tindex = getTimerIndex(index);

        _timeRecords[tindex] = now;
    }

    void TestContext::startPerfRecorder()
    {
        _perfRecorder.start();
    }

    void TestContext::stopPerfRecorder()
    {
        _perfRecorder.stop();
    }

    std::vector<std::uint64_t> TestContext::exportTimeRecords() const
    {
        std::vector<std::uint64_t> out(_timeRecords.size());
        std::transform(_timeRecords.begin(),
            _timeRecords.end(),
            out.begin(),
            [](std::optional<std::chrono::system_clock::time_point> const& record)
            {
                if (record)
                {
                    return std::chrono::duration_cast<UnsigendNanoseconds>(
                        record->time_since_epoch())
                        .count();
                }

                return std::numeric_limits<uint64_t>::max();
            });

        return out;
    }

    std::vector<std::uint64_t> TestContext::exportTimers() const
    {
        std::vector<std::uint64_t> out(_timers.size());
        std::transform(_timers.begin(),
            _timers.end(),
            out.begin(),
            [](TimerEntry const& te)
            {
                if (!te.isValid)
                {
                    return std::numeric_limits<std::uint64_t>::max();
                }

                return std::chrono::duration_cast<UnsigendNanoseconds>(te.stop - te.start).count();
            });

        return out;
    }

    std::vector<std::pair<std::string, std::string>> TestContext::exportPerfCounters()
    {
        return _perfRecorder.exportCounters();
    }

    void TestContext::resetTimers(std::size_t iterations) noexcept
    {
        _timeRecords.resize(iterations);
        _timers.resize(iterations);
        std::for_each(
            _timers.begin(), _timers.end(), [](TimerEntry& entry) { entry.isValid = false; });
        std::for_each(_timeRecords.begin(),
            _timeRecords.end(),
            [](std::optional<std::chrono::system_clock::time_point>& entry)
            { entry = std::nullopt; });
        _timerIndexOffset = std::numeric_limits<decltype(_timerIndexOffset)>::max();
    }

    bool TestContext::reflector() const noexcept
    {
        return !_isRunner;
    }

    bool TestContext::runner() const noexcept
    {
        return _isRunner;
    }

    Config const& TestContext::config() const noexcept
    {
        return _config;
    }

    void TestContext::resetFlows(std::string const& flowDef)
    {
        if (_flows)
        {
            _flows->destroy();
        }

        _flows.emplace(_config.domain, flowDef);
    }

    std::size_t TestContext::getTimerIndex(uint64_t index)
    {
        if (_timerIndexOffset == std::numeric_limits<decltype(_timerIndexOffset)>::max())
        {
            _timerIndexOffset = index;
        }

        uint64_t tindex = index - _timerIndexOffset;
        if (tindex >= _config.iterations)
        {
            throw std::runtime_error("timer index out of range");
        }

        return static_cast<std::size_t>(tindex);
    }
}
