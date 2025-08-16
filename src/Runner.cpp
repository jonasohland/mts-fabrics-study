#include "Runner.hpp"
#include <atomic>
#include <thread>
#include <httplib.h>
#include <uuid.h>
#include <fmt/format.h>
#include <picojson/picojson.h>
#include "internal/Logging.hpp"
#include "Defer.hpp"

namespace riedel::fabricsperf
{
    namespace http = httplib;

    Runner::Runner(Config const& config, std::unique_ptr<Test> test, std::string testName)
        : _interruped(false)
        , _test(std::move(test))
        , _testName(std::move(testName))
        , _flowSetup(config.domain, config.flowConfig())
        , _config(config)
        , _client(fmt::format("http://{}", config.connect))
    {}

    void Runner::run()
    {
        auto _ = defer([this]() { _flowSetup.destroy(); });

        createRemoteFlowSetup();
        initRemoteTest();
        pullRemoteTargetInfo();

        _test->setup(*this);
        _test->onRemoteEndpointAvailable(*this, *_remoteTargetInfo);
        _test->run(*this);
    }

    void Runner::createRemoteFlowSetup()
    {
        picojson::value v{};

        if (auto err = picojson::parse(v, _config.flowConfig()); !err.empty())
        {
            throw std::runtime_error(fmt::format("parse flow def: {}", err));
        }

        auto randomId = picojson::value(uuids::to_string(uuids::uuid_system_generator{}()));
        v.get("id").swap(randomId);

        MXL_INFO("New random flow id for remote setup: {}", v.get("id").to_str());

        auto remoteFlowDef = v.serialize();

        for (;;)
        {
            if (interrupted())
            {
                throw std::runtime_error("interrupted while trying to create remote flow setup");
            }

            auto result = _client.Post("/flow-def", remoteFlowDef, "application/json");
            if (auto err = result.error(); err != http::Error::Success)
            {
                MXL_ERROR("failed to set up remote flow: {}", http::to_string(err));
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            if (result->status != 200)
            {
                MXL_ERROR("failed to set up remote flow: {}", result->body);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            return;
        }
    }

    void Runner::initRemoteTest()
    {
        auto result = _client.Post("/init", _testName, "text/plain");
        if (auto err = result.error(); err != http::Error::Success)
        {
            throw std::runtime_error(
                fmt::format("failed to init remote test: {}", http::to_string(err)));
        }

        if (result->status != 200)
        {
            throw std::runtime_error(fmt::format("failed to init remote test: {}", result->body));
        }

        MXL_INFO("remote test initialized");
    }

    void Runner::pullRemoteTargetInfo()
    {
        auto result = _client.Get("/target-info");
        if (auto err = result.error(); err != http::Error::Success)
        {
            throw std::runtime_error(
                fmt::format("failed to pull remote target info: {}", http::to_string(err)));
        }

        if (result->status != 200)
        {
            throw std::runtime_error(
                fmt::format("failed to pull remote target info: {}", result->body));
        }

        _remoteTargetInfo = result->body;
        MXL_INFO("pulled {} bytes of remote target info", _remoteTargetInfo->length());
    }

    void Runner::stop()
    {
        _interruped.store(true, std::memory_order_relaxed);
    }

    bool Runner::reflector() const noexcept
    {
        return false;
    }

    bool Runner::runner() const noexcept
    {
        return true;
    }

    void Runner::timerStart(uint64_t index)
    {
        _timerStart = std::chrono::steady_clock::now();
    }

    void Runner::timerStop()
    {
        auto duration = std::chrono::steady_clock::now() - _timerStart;
        MXL_INFO("roundtrip: {}",
            std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
    }

    void Runner::setLocalTargetInfo(std::string info)
    {
        auto result = _client.Post("/target-info", info, "application/json");
        if (auto err = result.error(); err != http::Error::Success)
        {
            throw std::runtime_error(
                fmt::format("failed to post local target info: {}", http::to_string(err)));
        }

        if (result->status != 200)
        {
            throw std::runtime_error(
                fmt::format("failed to post local target info: {}", result->body));
        }
    }

    bool Runner::interrupted() const
    {
        return _interruped.load(std::memory_order_relaxed);
    }

    FlowSetup& Runner::flows()
    {
        return _flowSetup;
    }

    Config const& Runner::config() const
    {
        return _config;
    }

}
