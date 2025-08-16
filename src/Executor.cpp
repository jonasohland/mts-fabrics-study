#include "Executor.hpp"
#include <exception>
#include "internal/Logging.hpp"
#include "Reflector.hpp"
#include "Runner.hpp"

namespace riedel::fabricsperf
{
    Executor::Executor(Config config)
        : _stopped(false)
        , _config(std::move(config))
        , _factories()
        , _inner()
    {}

    void Executor::run()
    {
        if (_config.run == "list")
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
        if (_config.run == "all")
        {
            for (auto& [name, factory] : _factories)
            {
                if (_stopped.load())
                {
                    return;
                }

                _inner = std::make_unique<Runner>(_config, (*factory)(), name);
                _inner->run();
            }

            return;
        }

        auto test = (*_factories.at(_config.run))();
        _inner = std::make_unique<Runner>(_config, std::move(test), _config.run);
        _inner->run();
    }

    void Executor::reflector()
    {
        _inner = std::make_unique<Reflector>(_config, _factories);
        _inner->run();
    }

    void Executor::stop()
    {
        _stopped.store(true);
        if (_inner)
        {
            _inner->stop();
        }
    }

}
