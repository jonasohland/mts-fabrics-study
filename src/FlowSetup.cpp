#include "FlowSetup.hpp"
#include <stdexcept>
#include <uuid.h>
#include <mxl/flow.h>
#include <mxl/mxl.h>
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{
    void RegionsDeleter::operator()(mxlRegions_t* regions) const
    {
        mxlFabricsRegionsFree(regions);
    }

    FlowSetup::FlowSetup(std::string const& domain, std::string flowConfig)
    {
        _mxl = mxlCreateInstance(domain.c_str(), nullptr);
        if (_mxl == nullptr)
        {
            throw std::runtime_error("failed to create mxl instance");
        }

        if (mxlCreateFlow(_mxl, flowConfig.c_str(), nullptr, &_flowInfo) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to create flow");
        }

        auto flowId = uuids::to_string(_flowInfo.common.id);

        if (mxlCreateFlowWriter(_mxl, flowId.c_str(), nullptr, &_fw) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to create flow writer");
        }

        if (mxlCreateFlowReader(_mxl, flowId.c_str(), nullptr, &_fr) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to create flow reader");
        }

        MXL_INFO("Created flow setup [id={},grainCount={},rate={}/{}]",
            flowId,
            _flowInfo.discrete.grainCount,
            _flowInfo.discrete.grainRate.numerator,
            _flowInfo.discrete.grainRate.denominator);

        _flowConfig = std::move(flowConfig);
    }

    FlowSetup::~FlowSetup()
    {
        try
        {
            destroy();
        }
        catch (std::exception& ex)
        {
            MXL_ERROR("Failed to destroy flow setup: {}", ex.what());
        }
    }

    void FlowSetup::destroy()
    {
        bool anyCleanup = false;
        if (_fr)
        {
            if (mxlReleaseFlowReader(_mxl, _fr) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to release flow reader");
            }

            anyCleanup = true;
            _fr = nullptr;
        }

        if (_fw)
        {
            if (mxlReleaseFlowWriter(_mxl, _fw) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to release flow writer");
            }

            anyCleanup = true;
            _fw = nullptr;
        }

        if (_mxl)
        {
            if (mxlDestroyFlow(_mxl, uuids::to_string(_flowInfo.common.id).c_str()) !=
                MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to destroy flow");
            }
            if (mxlDestroyInstance(_mxl) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to destroy mxl instance");
            }

            anyCleanup = true;
            _mxl = nullptr;
        }

        if (anyCleanup)
        {
            MXL_INFO("Flow setup cleaned up");
        }
    }

    MxlRegions FlowSetup::getWriterRegions()
    {
        mxlRegions regions;
        if (mxlFabricsRegionsForFlowWriter(_fw, &regions) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to get regions from flow data");
        }

        return MxlRegions{regions, RegionsDeleter{}};
    }

    MxlRegions FlowSetup::getReaderRegions()
    {
        mxlRegions regions;
        if (mxlFabricsRegionsForFlowReader(_fr, &regions) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to get regions from flow data");
        }

        return MxlRegions{regions, RegionsDeleter{}};
    }
}
