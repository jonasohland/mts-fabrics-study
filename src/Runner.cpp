#include "Runner.hpp"
#include <atomic>
#include <iterator>
#include <thread>
#include <httplib.h>
#include <uuid.h>
#include <fmt/format.h>
#include <picojson/picojson.h>
#include "internal/Logging.hpp"
#include "CSV.hpp"

namespace riedel::fabricsperf
{
    namespace http = httplib;

    Runner::Runner(Config const& config, std::unique_ptr<Test> test, std::string testName)
        : TestContext(config)
        , _interruped(false)
        , _test(std::move(test))
        , _testName(std::move(testName))
        , _config(config)
        , _client(fmt::format("http://{}", config.connect))
    {
        resetFlows(config.flowConfig());
    }

    void Runner::run()
    {
        createRemoteFlowSetup();
        initRemoteTest();
        pullRemoteTargetInfo();

        _test->setup(*this);

        if (_test->needsReflector())
        {
            _test->onRemoteEndpointAvailable(*this, *_remoteTargetInfo);
        }

        _test->run(*this);
        _test->teardown(*this);
    }

    void Runner::createRemoteFlowSetup()
    {
        if (!_test->needsReflector())
        {
            return;
        }

        picojson::value v{};

        if (auto err = picojson::parse(v, _config.flowConfig()); !err.empty())
        {
            throw std::runtime_error(fmt::format("parse flow def: {}", err));
        }

        auto randomId = picojson::value(uuids::to_string(uuids::uuid_system_generator{}()));
        v.get("id").swap(randomId);

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
        if (!_test->needsReflector())
        {
            return;
        }

        auto result = _client.Post(
            "/init", fmt::format("{},{}", _testName, _config.iterations), "text/plain");
        if (auto err = result.error(); err != http::Error::Success)
        {
            throw std::runtime_error(
                fmt::format("failed to init remote test: {}", http::to_string(err)));
        }

        if (result->status != 200)
        {
            throw std::runtime_error(fmt::format("failed to init remote test: {}", result->body));
        }
    }

    void Runner::pullRemoteTargetInfo()
    {
        if (!_test->needsReflector())
        {
            _remoteTargetInfo = "No reflector available for this test";

            return;
        }

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

    void Runner::setLocalTargetInfo(std::string info)
    {
        if (!_test->needsReflector())
        {
            return;
        }

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

    void Runner::signalReady()
    {
        _localIsReady = true;
    }

    bool Runner::remoteIsReady()
    {
        if (!_test->needsReflector())
        {
            return true;
        }

        if (_localIsReady)
        {
            http::Result result;
            result = _client.Post("/ready");
            if (auto err = result.error(); err != http::Error::Success)
            {
                MXL_WARN("failed to post local readiness state: {}", http::to_string(err));
                return false;
            }

            // remote responds with 200 to our update when it is ready
            if (result->status == 200)
            {
                return true;
            }
        }

        return false;
    }

    std::array<std::vector<std::string>, 3> Runner::exportResults()
    {
        auto itimers = exportTimers();
        auto ilocalTimeRecords = exportTimeRecords();

        std::vector<std::string> timers{};
        std::vector<std::string> localTimeRecords{};
        std::vector<std::string> remoteTimeRecords{};

        if (_test->needsReflector())
        {
            csv::Reader reader;
            auto result = _client.Get("/time-records");
            if (auto err = result.error(); err != http::Error::Success)
            {
                throw std::runtime_error(
                    std::format("failed to get remote time records: {}", http::to_string(err)));
            }

            reader.parse(result->body);
            for (auto const& val : *reader.begin())
            {
                // this hurts
                val.read_value(remoteTimeRecords.emplace_back());
            }
        }
        else
        {
            auto fillVal = std::to_string(std::numeric_limits<uint64_t>::max());
            remoteTimeRecords.resize(ilocalTimeRecords.size(), fillVal);
        }

        std::transform(itimers.begin(),
            itimers.end(),
            std::back_inserter(timers),
            [](uint64_t val) { return std::to_string(val); });

        std::transform(ilocalTimeRecords.begin(),
            ilocalTimeRecords.end(),
            std::back_inserter(localTimeRecords),
            [](uint64_t val) { return std::to_string(val); });

        return {timers, localTimeRecords, remoteTimeRecords};
    }
}
