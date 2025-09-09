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
        ~Reflector() override = default;

        enum class TestState
        {
            SETUP,
            RUN,
            TEARDOWN
        };

        void run() override;
        void stop() override;

        void onRemoteTargetInfo(std::string targetInfo);
        std::string getLocalTargetInfo();

        // TestContext
        void setLocalTargetInfo(std::string info) override;
        bool interrupted() const override;
        void signalReady() override;
        bool remoteIsReady() override;

    private:
        void reset(std::size_t iterations);
        void initTest(std::string testName);

        void runTest();

        std::mutex _m{};
        std::condition_variable _c{};

        std::unique_ptr<Test> _test{nullptr};
        TestState _state{TestState::SETUP};
        std::optional<std::string> _localTargetInfo{std::nullopt};
        std::optional<std::string> _remoteTargetInfo{std::nullopt};
        std::optional<std::thread> _testThread{std::nullopt};
        std::optional<FlowSetup> _flowSetup{std::nullopt};

        std::atomic_bool _interrupted{false};
        bool _remoteIsReady{false};
        bool _localIsReady{false};

        Config const& _config;
        std::unordered_map<std::string, std::unique_ptr<TestFactory>> const& _factories;
        httplib::Server _srv;
    };
}
