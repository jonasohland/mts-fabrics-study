#pragma once

#include <optional>
#include <httplib.h>
#include "Config.hpp"
#include "FlowSetup.hpp"
#include "Test.hpp"

namespace riedel::fabricsperf
{
    class Reflector : public TestContext
    {
    public:
        Reflector(Config const&,
            std::unordered_map<std::string, std::unique_ptr<TestFactory>> const&);

        void run();
        void init(std::string testName);

        void onRemoteTargetInfo(std::string targetInfo);
        std::string getLocalTargetInfo();

        // TestContext
        virtual void timerStart(uint64_t index) = 0;
        virtual void timerStop() = 0;
        virtual void setLocalTargetInfo(std::string info) = 0;
        virtual bool interrupted() const = 0;

    private:
        void initFlows();

        void runTest();

        std::mutex _m{};
        std::condition_variable _c{};

        std::unique_ptr<Test> _test{nullptr};
        std::optional<std::string> _localTargetInfo{std::nullopt};
        std::optional<std::thread> _testThread{std::nullopt};
        std::optional<FlowSetup> _flowSetup{std::nullopt};

        Config const& _config;
        std::unordered_map<std::string, std::unique_ptr<TestFactory>> const& _factories;
        httplib::Server _srv;
    };
}
