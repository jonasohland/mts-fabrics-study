#include "Test.hpp"
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{

    TestContext::TestContext(Config const& config)
        : _isRunner(config.mode() == Mode::RUNNER)
        , _config(config)
    {}

    TestContext::~TestContext()
    {
        if (_flows)
        {
            try
            {
                _flows->destroy();
            }
            catch (std::exception& ex)
            {
                MXL_ERROR("failed to destroy flow setup: {}", ex.what());
            }
        }
    }

    FlowSetup& TestContext::flows()
    {
        if (!_flows)
        {
            throw std::runtime_error("flows not set up");
        }

        return *_flows;
    }

    bool TestContext::reflector() const noexcept
    {
        return !_isRunner;
    }

    bool TestContext::runner() const noexcept
    {
        return _isRunner;
    }

    Config const& TestContext::config() const noexcept
    {
        return _config;
    }

    void TestContext::resetFlows(std::string const& flowDef)
    {
        if (_flows)
        {
            _flows->destroy();
        }

        _flows.emplace(_config.domain, flowDef);
    }
}
