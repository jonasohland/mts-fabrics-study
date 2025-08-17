#pragma once

#include <optional>
#include <httplib.h>
#include "Config.hpp"
#include "Executor.hpp"
#include "Test.hpp"

namespace riedel::fabricsperf
{
    class Runner
        : public TestContext
        , public Executor::Inner
    {
    public:
        Runner(Config const& config, std::unique_ptr<Test> test, std::string testName);

        void stop() override;
        void run() override;

        // TestContext
        void timerStart(uint64_t index) override;
        void timerStop() override;
        void setLocalTargetInfo(std::string info) override;
        bool interrupted() const override;

    private:
        void createRemoteFlowSetup();
        void initRemoteTest();
        void pullRemoteTargetInfo();

        std::chrono::steady_clock::time_point _timerStart;

        std::optional<std::string> _remoteTargetInfo{};

        std::atomic_bool _interruped;

        std::unique_ptr<Test> _test;
        std::string _testName;

        Config const& _config;
        httplib::Client _client;
    };
}
