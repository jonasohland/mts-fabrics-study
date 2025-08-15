#include "Executor.hpp"
#include <exception>
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
        _inner = std::make_unique<Runner>(_config);
        _inner->run();
    }

    void Executor::reflector()
    {
        _inner = std::make_unique<Reflector>(_config, _factories);
        _inner->run();
    }

    void Executor::stop()
    {
        if (_inner)
        {
            _inner->stop();
        }
    }

}
