#pragma once

#include <memory>
#include <string>
#include <mxl/flow.h>
#include <mxl/mxl.h>
#include "Config.hpp"
#include "FlowSetup.hpp"

namespace riedel::fabricsperf
{
    class TestContext
    {
    public:
        TestContext();

        [[nodiscard]]
        virtual bool reflector() const noexcept = 0;
        [[nodiscard]]
        virtual bool runner() const noexcept = 0;

        virtual void timerStart(uint64_t index) = 0;
        virtual void timerStop() = 0;
        virtual void setLocalTargetInfo(std::string info) = 0;
        [[nodiscard]]
        virtual bool interrupted() const = 0;
        virtual FlowSetup& flows() = 0;
        [[nodiscard]]
        virtual Config const& config() const = 0;
    };

    class Test
    {
    public:
        virtual ~Test() = default;

        virtual void setup(TestContext& ctx) = 0;
        virtual void teardown(TestContext& ctx) = 0;
        virtual void run(TestContext& ctx) = 0;
        virtual void onRemoteEndpointAvailable(TestContext& ctx, std::string info) = 0;
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
