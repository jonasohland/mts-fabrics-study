#pragma once

#include <optional>
#include <httplib.h>
#include "Config.hpp"
#include "Executor.hpp"
#include "FlowSetup.hpp"
#include "Test.hpp"

namespace riedel::fabricsperf
{
    class Reflector
        : public TestContext
        , public Executor::Inner
    {
    public:
        Reflector(Config const&,
            std::unordered_map<std::string, std::unique_ptr<TestFactory>> const&);
        virtual ~Reflector() = default;

        void run() override;
        void stop() override;

        void onRemoteTargetInfo(std::string targetInfo);
        std::string getLocalTargetInfo();

        // TestContext
        bool reflector() const noexcept override;
        bool runner() const noexcept override;
        void timerStart(uint64_t index) override;
        void timerStop() override;
        void setLocalTargetInfo(std::string info) override;
        bool interrupted() const override;
        FlowSetup& flows() override;

    private:
        void terminateTest();
        void initTest(std::string testName);
        void initFlow(std::string flowDef);

        void runTest();

        std::mutex _m{};
        std::condition_variable _c{};

        std::unique_ptr<Test> _test{nullptr};
        std::optional<std::string> _localTargetInfo{std::nullopt};
        std::optional<std::string> _remoteTargetInfo{std::nullopt};
        std::optional<std::thread> _testThread{std::nullopt};
        std::optional<FlowSetup> _flowSetup{std::nullopt};
        std::atomic_bool _interrupted{false};

        Config const& _config;
        std::unordered_map<std::string, std::unique_ptr<TestFactory>> const& _factories;
        httplib::Server _srv;
    };
}
