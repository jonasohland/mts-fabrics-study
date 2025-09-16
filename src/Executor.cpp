#include "Executor.hpp"
#include <exception>
#include "Output.hpp"
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
        auto mode = _config.mode();
        switch (mode)
        {
            case Mode::RUNNER: {
                if (_config.run.empty())
                {
                    for (auto const& [name, _] : _factories)
                    {
                        std::cout << name << '\n';
                    }

                    return;
                }

                runner();
                break;
            }
            case Mode::REFLECTOR: reflector(); break;
            default:              std::terminate();
        }
    }

    void Executor::runner()
    {
        Results results;
        PerfCounters counters;
        PcieCounters nvmlCounters;

        for (auto const& testCase : _config.run)
        {
            if (_stopped.load())
            {
                return;
            }

            auto test = (*_factories.at(testCase))();
            _inner = std::make_unique<Runner>(_config, std::move(test), testCase);
            _inner->run();

            results.emplace_back(testCase, dynamic_cast<Runner&>(*_inner).exportResults());
            counters.emplace(testCase, dynamic_cast<Runner&>(*_inner).exportPerfCounters());
            nvmlCounters.emplace(testCase, dynamic_cast<Runner&>(*_inner).exportNvmlPcieCounters());

            _inner.reset(); // Destroy the previous Runner and all its child when we are done with
                            // them.
        }

        writeResults(_config.output, results);
        writePerfCounters(_config.output, counters);
        writeNvmlPcieCounters(_config.output, nvmlCounters);
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
