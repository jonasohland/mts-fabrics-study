#include "FlowSetup.hpp"
#include <stdexcept>
#include <mxl/flow.h>
#include "mxl/mxl.h"

namespace riedel::fabricsperf
{
    void RegionsDeleter::operator()(mxlRegions_t* regions) const
    {
        mxlFabricsRegionsFree(regions);
    }

    FlowSetup::FlowSetup(std::string const& domain, std::string flowInfo)
    {
        _mxl = mxlCreateInstance(domain.c_str(), nullptr);
        if (_mxl != nullptr)
        {
            throw std::runtime_error("failed to create flow instance");
        }
    }

    FlowSetup::~FlowSetup()
    {
        if (mxlReleaseFlowReader(_mxl, _fr) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to release flow reader");
        }
        if (mxlReleaseFlowWriter(_mxl, _fw) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to release flow writer");
        }
        if (mxlDestroyInstance(_mxl) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to destroy mxl instance");
        }
    }

    MxlRegions FlowSetup::getRegions()
    {
        mxlRegions regions;
        if (mxlFabricsRegionsFromFlow(nullptr, &regions) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to get regions from flow data");
        }

        return MxlRegions{regions, RegionsDeleter{}};
    }
}
