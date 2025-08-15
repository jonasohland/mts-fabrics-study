#include "Reflector.hpp"
#include <uuid.h>

namespace riedel::fabricsperf
{
    namespace http = httplib;

    Reflector::Reflector(Config const& config,
        std::unordered_map<std::string, std::unique_ptr<TestFactory>> const& factories)
        : _config(config)
        , _factories(factories)
        , _srv()
    {}

    void Reflector::run()
    {
        _srv.Post("/init",
            [this](http::Request const& req, http::Response&) { initTest(req.body); });
        _srv.Post("/flow-def",
            [this](http::Request const& req, http::Response&) { initFlow(req.body); });
        _srv.Post("/target-info",
            [this](http::Request const& req, http::Response&) { onRemoteTargetInfo(req.body); });
        _srv.Get("/target-info",
            [this](http::Request const&, http::Response& res)
            { res.set_content(getLocalTargetInfo(), "application/json"); });

        _srv.listen(_config.listenHost(), _config.listenPort());

        terminateTest();
    }

    void Reflector::stop()
    {
        _srv.stop();
    }

    void Reflector::onRemoteTargetInfo(std::string info)
    {
        std::unique_lock _l{_m};

        _remoteTargetInfo.emplace(std::move(info));
        _c.notify_all();
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

            _c.wait(l, [this]() { return !_test && _localTargetInfo; });
        }
    }

    bool Reflector::reflector() const noexcept
    {
        return true;
    }

    bool Reflector::runner() const noexcept
    {
        return false;
    }

    void Reflector::timerStart(uint64_t)
    {}

    void Reflector::timerStop()
    {}

    void Reflector::setLocalTargetInfo(std::string info)
    {
        std::unique_lock _l{_m};

        if (!_test)
        {
            throw std::runtime_error("no test is running");
        }

        _localTargetInfo = info;
        _c.notify_all();
    }

    bool Reflector::interrupted() const
    {
        return _interrupted.load(std::memory_order_relaxed);
    }

    FlowSetup& Reflector::flows()
    {
        if (!_flowSetup)
        {
            throw std::runtime_error{"flows not configured"};
        }

        return *_flowSetup;
    }

    void Reflector::initTest(std::string testName)
    {
        std::unique_lock _l{_m};
        terminateTest();

        if (testName != "")
        {
            // call test factory for new test
            _test = (*_factories.at(testName))();

            _testThread.emplace(std::bind(&Reflector::runTest, this));
        }

        _c.notify_all();
    }

    void Reflector::initFlow(std::string flowDef)
    {
        std::unique_lock _l{_m};
        terminateTest();

        _flowSetup.emplace(_config.domain, flowDef);
    }

    void Reflector::terminateTest()
    {
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
        }

        // call the test teardown function
        if (_test)
        {
            _test->teardown(*this);
            _test.reset();
        }

        if (_flowSetup)
        {
            _flowSetup->destroy();
            _flowSetup.reset();
        }
    }

    void Reflector::runTest()
    {
        if (_test)
        {
            _test->run(*this);
        }
    }
}
