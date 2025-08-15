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
        _srv.new_task_queue = []() -> http::TaskQueue*
        {
            return new http::ThreadPool(1);
        };

        _srv.Post("/init", [this](http::Request const& req, http::Response&) { init(req.body); });
        _srv.Post("/target-info",
            [this](http::Request const& req, http::Response&) { onRemoteTargetInfo(req.body); });
        _srv.Get("/target-info",
            [this](http::Request const&, http::Response& res)
            { res.set_content(getLocalTargetInfo(), "application/json"); });

        _srv.listen(_config.listenHost(), _config.listenPort());
    }

    void Reflector::init(std::string testName)
    {
        std::unique_lock _l{_m};
        _localTargetInfo = std::nullopt;
        _test = (*_factories.at(testName))();
        _testThread.emplace(std::bind(&Reflector::runTest, this));
        _c.notify_all();
    }

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

    void Reflector::initFlows()
    {
        _mxl = mxlCreateInstance(_config.domain.c_str(), nullptr);
        if (!_mxl)
        {
            throw std::runtime_error("mxl instance creation failed");
        }

        mxlCreateFlow(_mxl, "", "", &_frInfo);
        mxlCreateFlow(_mxl, "", "", &_fwInfo);

        uuids::uuid frid{_frInfo.common.id};
        uuids::uuid fwid{_frInfo.common.id};

        mxlCreateFlowReader(_mxl, uuids::to_string(frid).c_str(), nullptr, &_fr);
        mxlCreateFlowWriter(_mxl, uuids::to_string(fwid).c_str(), nullptr, &_fw);
    }

    void Reflector::runTest()
    {}
}
