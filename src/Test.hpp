#pragma once

#include <memory>
#include <optional>
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
        TestContext(Config const&);
        virtual ~TestContext();

        virtual void timerStart(uint64_t index) = 0;
        virtual void timerStop() = 0;
        virtual void setLocalTargetInfo(std::string info) = 0;
        [[nodiscard]]
        virtual bool interrupted() const = 0;

        FlowSetup& flows();

        [[nodiscard]]
        bool reflector() const noexcept;
        [[nodiscard]]
        bool runner() const noexcept;
        [[nodiscard]]
        Config const& config() const noexcept;

    protected:
        void resetFlows(std::string const& flowDef);

    private:
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
