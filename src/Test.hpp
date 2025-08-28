#pragma once

#include <memory>
#include <optional>
#include <string>
#include <mxl/flow.h>
#include <mxl/mxl.h>
#include "Config.hpp"
#include "FlowSetup.hpp"
#include "Perf.hpp"

namespace riedel::fabricsperf
{
    class TestContext
    {
    public:
        using UnsigendNanoseconds = std::chrono::duration<std::uint64_t, std::nano>;

        TestContext(Config const&);
        virtual ~TestContext();

        virtual void setLocalTargetInfo(std::string info) = 0;
        [[nodiscard]]
        virtual bool interrupted() const = 0;
        virtual void signalReady() = 0;
        virtual bool remoteIsReady() = 0;

        FlowSetup& flows();

        void timerStart(uint64_t index);
        void timerStop(uint64_t index);
        void recordCurrentTime(uint64_t index);
        void startPerfRecorder();
        void stopPerfRecorder();

        [[nodiscard]]
        bool reflector() const noexcept;
        [[nodiscard]]
        bool runner() const noexcept;
        [[nodiscard]]
        Config const& config() const noexcept;

        void resetTimers(std::size_t iterations) noexcept;
        std::vector<std::uint64_t> exportTimeRecords() const;
        std::vector<std::uint64_t> exportTimers() const;
        std::vector<std::pair<std::string, std::string>> exportPerfCounters();

    protected:
        void resetFlows(std::string const& flowDef);

    private:
        std::size_t getTimerIndex(uint64_t);

        struct TimerEntry
        {
            bool isValid;
            std::chrono::system_clock::time_point start;
            std::chrono::system_clock::time_point stop;
        };

        std::vector<TimerEntry> _timers;
        std::vector<std::optional<std::chrono::system_clock::time_point>> _timeRecords;
        uint64_t _timerIndexOffset = std::numeric_limits<uint64_t>::max();

        PerfRecorder _perfRecorder{};
        std::optional<FlowSetup> _flows{};
        bool _isRunner;
        Config const& _config;
    };

    class Test
    {
    public:
        virtual ~Test() = default;

        virtual void setup(TestContext& ctx) = 0;
        virtual void teardown(TestContext& ctx) = 0;
        virtual void run(TestContext& ctx) = 0;
        virtual void onRemoteEndpointAvailable(TestContext& ctx, std::string info) = 0;
        virtual bool needsReflector() const = 0;
    };

    class TestFactory
    {
    public:
        virtual ~TestFactory() = default;

        [[nodiscard]]
        virtual std::string name() const = 0;
        virtual std::unique_ptr<Test> operator()() const = 0;
    };

} // namespace riedel::fabricsperf
