#pragma once

#include <httplib.h>
#include <unordered_map>
#include "Config.hpp"
#include "Test.hpp"

namespace riedel::fabricsperf
{
    class Executor
    {
    public:
        Executor(Config);

        template<typename Test>
        void add()
        {
            auto factory = std::make_unique<typename Test::Factory>();
            auto name = factory->name();

            _factories.emplace(name, std::move(factory));
        }

        void run();

    private:
        void runner();
        void reflector();

        Config _config;
        std::unordered_map<std::string, std::unique_ptr<TestFactory>> _factories;
    };
} // namespace riedel::fabricsperf
