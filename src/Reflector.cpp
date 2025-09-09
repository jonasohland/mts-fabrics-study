#include "Reflector.hpp"
#include <cstdlib>
#include <sstream>
#include <uuid.h>
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{
    namespace http = httplib;

    Reflector::Reflector(Config const& config,
        std::unordered_map<std::string, std::unique_ptr<TestFactory>> const& factories)
        : TestContext(config)
        , _config(config)
        , _factories(factories)
        , _srv()
    {}

    void Reflector::run()
    {
        _srv.Post("/init",
            [this](http::Request const& req, http::Response& res)
            {
                try
                {
                    initTest(req.body);
                }
                catch (std::exception& ex)
                {
                    res.status = 500;
                    res.body = ex.what();
                }
            });
        _srv.Post("/flow-def",
            [this](http::Request const& req, http::Response& res)
            {
                try
                {
                    reset(0);
                    resetFlows(req.body);
                }
                catch (std::exception& ex)
                {
                    res.status = 500;
                    res.body = ex.what();
                }
            });
        _srv.Post("/target-info",
            [this](http::Request const& req, http::Response&) { onRemoteTargetInfo(req.body); });
        _srv.Get("/target-info",
            [this](http::Request const&, http::Response& res)
            { res.set_content(getLocalTargetInfo(), "application/json"); });
        _srv.Post("/ready",
            [this](http::Request const&, http::Response& response)
            {
                if (_localIsReady)
                {
                    response.status = 200;
                }
                else
                {
                    response.status = 412;
                }

                _remoteIsReady = true;
            });
        _srv.Get("/time-records",
            [this](http::Request const&, http::Response& response)
            {
                auto values = exportTimeRecords();
                if (values.empty())
                {
                    return;
                }

                std::stringstream ss{};
                std::copy(
                    values.begin(), values.end() - 1, std::ostream_iterator<uint64_t>{ss, ","});
                ss << values.back();

                response.body = ss.str();
            });

        MXL_INFO(
            "Starting reflector server on {}:{} ...", _config.listenHost(), _config.listenPort());

        // will block until interrupted by Reflector::stop()
        _srv.listen(_config.listenHost(), _config.listenPort());

        reset(0);

        MXL_INFO("Reflector server shut down");
    }

    void Reflector::stop()
    {
        _srv.stop();
    }

    void Reflector::onRemoteTargetInfo(std::string info)
    {
        std::unique_lock _l{_m};

        if (!_test)
        {
            throw std::runtime_error("no test active");
        }

        _test->onRemoteEndpointAvailable(*this, std::move(info));
    }

    std::string Reflector::getLocalTargetInfo()
    {
        std::unique_lock l{_m};

        for (;;)
        {
            if (!_test)
            {
                throw std::runtime_error("test aborted");
            }

            if (_localTargetInfo)
            {
                return *_localTargetInfo;
            }

            _c.wait(l);
        }
    }

    void Reflector::setLocalTargetInfo(std::string info)
    {
        std::unique_lock _l{_m};

        _localTargetInfo = info;
    }

    bool Reflector::interrupted() const
    {
        return _interrupted.load(std::memory_order_relaxed);
    }

    void Reflector::signalReady()
    {
        _localIsReady = true;
    }

    bool Reflector::remoteIsReady()
    {
        return _remoteIsReady;
    }

    void Reflector::initTest(std::string testInfo)
    {
        std::unique_ptr<Test> test;

        auto testName = testInfo.substr(0, testInfo.find(','));
        auto iterations = std::stoul(testInfo.substr(testInfo.find(',') + 1, testInfo.length()));
        MXL_INFO("starting test '{}', expecting {} iterations", testName, iterations);

        reset(iterations);

        if (testInfo != "")
        {
            // call test factory for new test
            test = (*_factories.at(testName))();
        }

        _test = std::move(test);
        _interrupted.store(false, std::memory_order_relaxed);
        _testThread.emplace([this]() { this->runTest(); });
        _c.notify_all();
    }

    void Reflector::reset(std::size_t iterations)
    {
        MXL_INFO("Resetting reflector implementation");

        // reset target infos
        _localTargetInfo.reset();
        _remoteTargetInfo.reset();

        _remoteIsReady = false;
        _localIsReady = false;

        // signal the test thread to exit
        _interrupted.store(true, std::memory_order_relaxed);

        // cancel and join possibly still running test thread
        if (_testThread)
        {
            _testThread->join();
            _testThread.reset();
        }

        resetTimers(iterations);
    }

    void Reflector::runTest()
    {
        try
        {
            _test->setup(*this);
            {
                std::unique_lock lk{_m};
                _c.notify_all();
            }
            _test->run(*this);
            {
                std::unique_lock lk{_m};
                _c.notify_all();
            }
        }
        catch (std::exception& ex)
        {
            MXL_ERROR("Failed to run reflector implementation for test: {}", ex.what());
        }

        {
            std::unique_lock lk{_m};
            _c.notify_all();
        }
        _test->teardown(*this);
    }
}
