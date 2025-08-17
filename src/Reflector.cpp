#include "Reflector.hpp"
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
            [this](http::Request const& req, http::Response&) { initTest(req.body); });
        _srv.Post("/flow-def",
            [this](http::Request const& req, http::Response&)
            {
                reset();
                resetFlows(req.body);
            });
        _srv.Post("/target-info",
            [this](http::Request const& req, http::Response&) { onRemoteTargetInfo(req.body); });
        _srv.Get("/target-info",
            [this](http::Request const&, http::Response& res)
            { res.set_content(getLocalTargetInfo(), "application/json"); });

        MXL_INFO(
            "Starting reflector server on {}:{} ...", _config.listenHost(), _config.listenPort());

        // will block until interrupted by Reflector::stop()
        _srv.listen(_config.listenHost(), _config.listenPort());

        reset();

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

    void Reflector::timerStart(uint64_t)
    {}

    void Reflector::timerStop()
    {}

    void Reflector::setLocalTargetInfo(std::string info)
    {
        std::unique_lock _l{_m};

        _localTargetInfo = info;
    }

    bool Reflector::interrupted() const
    {
        return _interrupted.load(std::memory_order_relaxed);
    }

    void Reflector::initTest(std::string testName)
    {
        std::unique_ptr<Test> test;
        {
            std::unique_lock _l{_m};

            reset();

            if (testName != "")
            {
                // call test factory for new test
                test = (*_factories.at(testName))();
            }

            _c.notify_all();
        }

        test->setup(*this);

        {
            std::unique_lock _l{_m};

            _test = std::move(test);
            _interrupted.store(false, std::memory_order_relaxed);
            _testThread.emplace([this]() { this->runTest(); });
            _c.notify_all();
        }
    }

    void Reflector::reset()
    {
        MXL_INFO("Resetting reflector implementation");

        // reset target infos
        _localTargetInfo.reset();
        _remoteTargetInfo.reset();

        // signal the test thread to exit
        _interrupted.store(true, std::memory_order_relaxed);

        // cancel and join possibly still running test thread
        if (_testThread)
        {
            _testThread->join();
            _testThread.reset();

            _test->teardown(*this);
        }
    }

    void Reflector::runTest()
    {
        try
        {
            _test->run(*this);
        }
        catch (std::exception& ex)
        {
            MXL_ERROR("Failed to run reflector implementation for test: {}", ex.what());
        }
    }
}
