#pragma once

#include <memory>
#include <string>
#include <mxl/flow.h>
#include <mxl/mxl.h>

namespace riedel::fabricsperf
{
    class TestContext
    {
    public:
        TestContext();

        virtual bool reflector() const noexcept = 0;
        virtual bool runner() const noexcept = 0;

        virtual void timerStart(uint64_t index) = 0;
        virtual void timerStop() = 0;
        virtual void setLocalTargetInfo(std::string info) = 0;
        virtual bool interrupted() const = 0;
    };

    class Test
    {
    public:
        virtual ~Test() = default;

        virtual void setup(TestContext& ctx, mxlFlowReader* reader, mxlFlowWriter* writer) = 0;
        virtual void teardown(TestContext& ctx) = 0;
        virtual void run(TestContext& ctx) = 0;
        virtual void onRemoteEndpointAvailable(TestContext& ctx, std::string info) = 0;
    };

    class TestFactory
    {
    public:
        virtual ~TestFactory() = default;

        virtual std::string name() const = 0;
        virtual std::unique_ptr<Test> operator()() const = 0;
    };

} // namespace riedel::fabricsperf
