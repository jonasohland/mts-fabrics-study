#include "MXLHost2Host.hpp"
#include "mxl/fabrics.h"

namespace riedel::fabricsperf
{
    std::unique_ptr<Test> MXLHost2HostFactory::operator()() const
    {
        return std::make_unique<MXLHost2Host>();
    }

    std::string MXLHost2HostFactory::name() const
    {
        return "MXLHost2Host";
    }

    void MXLHost2Host::setup(TestContext& ctx)
    {
        if (mxlFabricsCreateInstance(ctx.flows().instance(), &_instance) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to create farbrics instance");
        }

        if (mxlFabricsCreateTarget(_instance, &_tg) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to create target");
        }

        if (mxlFabricsCreateInitiator(_instance, &_in) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to create target");
        }

        auto regions = ctx.flows().getReaderRegions();
        auto config = mxlInitiatorConfig{
            .endpointAddress = mxlEndpointAddress{},
            .provider = MXL_SHARING_PROVIDER_VERBS,
            .regions = regions.get(),
        };

        if (mxlFabricsInitiatorSetup(_in, &config) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to set up initiator");
        }
    }

    void MXLHost2Host::teardown(TestContext&)
    {}

    void MXLHost2Host::run(TestContext&)
    {}

    void MXLHost2Host::onRemoteEndpointAvailable(TestContext& ctx, std::string)
    {}

}
