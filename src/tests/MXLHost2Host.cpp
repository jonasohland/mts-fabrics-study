#include "MXLHost2Host.hpp"

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

    void MXLHost2Host::setup(mxlFlowReader*, mxlFlowWriter*)
    {}

    void MXLHost2Host::teardown()
    {}

    void MXLHost2Host::run(TestContext&)
    {}

    void MXLHost2Host::onRemoteEndpointAvailable(std::string)
    {}

}
