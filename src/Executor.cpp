#include "Executor.hpp"
#include <exception>
#include "internal/Logging.hpp"
#include "Reflector.hpp"
#include "Runner.hpp"

namespace riedel::fabricsperf
{
    Executor::Executor(Config config)
        : _config(std::move(config))
    {}

    void Executor::run()
    {
        if (_config.test == "list")
        {
            for (auto const& [name, _] : _factories)
            {
                std::cout << name << '\n';
            }

            return;
        }

        auto mode = _config.mode();
        switch (mode)
        {
            case Mode::RUNNER:    runner(); break;
            case Mode::REFLECTOR: reflector(); break;
            default:              std::terminate();
        }
    }

    void Executor::runner()
    {
        Runner runner{_config};
        runner.run();
    }

    void Executor::reflector()
    {
        MXL_INFO("Running reflector mode");
        Reflector reflector{_config, _factories};
        reflector.run();
    }
}
