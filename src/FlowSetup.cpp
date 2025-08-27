#include "FlowSetup.hpp"
#include <cstdint>
#include <stdexcept>
#include <uuid.h>
#include <cuda_runtime.h>
#include <mxl/flow.h>
#include <mxl/mxl.h>
#include "../mxl/lib/src/internal/FlowParser.hpp"
#include "internal/Logging.hpp"
#include "Region.hpp"

namespace riedel::fabricsperf
{

    MXLGrainRegion grainRegion(MXLRegions& mxlRegions, uint64_t index)
    {
        auto regionGroups = reinterpret_cast<ofi::RegionGroups*>(mxlRegions.get());
        auto regionGroupsVec = regionGroups->view();
        auto regionGroup = regionGroupsVec[index % regionGroupsVec.size()];
        auto regions = regionGroup.view();

        if (regions.size() != 1)
        {
            throw std::runtime_error("unexpected regions groups count");
        }

        return {regions[0].base, regions[0].size, regions[0].loc};
    }

    std::vector<MXLGrainRegion> grainRegions(MXLRegions& mxlRegions)
    {
        std::vector<MXLGrainRegion> out{};
        auto regionGroups = reinterpret_cast<ofi::RegionGroups*>(mxlRegions.get());

        for (auto const& rgroup : regionGroups->view())
        {
            auto vec = rgroup.view();
            if (vec.size() != 1)
            {
                throw std::runtime_error("encountered a region group with more than 1 region");
            }

            out.emplace_back(vec[0].base, vec[0].size, vec[0].loc);
        }

        return out;
    }

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

        if (_cudaWriterBuf)
        {
            if (cudaFree(_cudaWriterBuf) != cudaSuccess)
            {
                throw std::runtime_error("failed to free writer cuda buffer");
            }

            anyCleanup = true;
            _cudaWriterBuf = nullptr;
        }

        if (_cudaReaderBuf)
        {
            if (cudaFree(_cudaReaderBuf) != cudaSuccess)
            {
                throw std::runtime_error("failed to free reader cuda buffer");
            }

            anyCleanup = true;
            _cudaReaderBuf = nullptr;
        }

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

    mxlFlowInfo const& FlowSetup::flowInfo() const noexcept
    {
        return _flowInfo;
    }

    uuids::uuid FlowSetup::localFlowId() noexcept
    {
        return _flowInfo.common.id;
    }

    mxlFlowReader FlowSetup::reader() noexcept
    {
        return _fr;
    }

    mxlFlowWriter FlowSetup::writer() noexcept
    {
        return _fw;
    }

    MXLRegions FlowSetup::getWriterRegions()
    {
        mxlRegions regions;
        if (mxlFabricsRegionsForFlowWriter(_fw, &regions) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to get regions from flow data");
        }

        return MXLRegions{regions, RegionsDeleter{}};
    }

    MXLRegions FlowSetup::getReaderRegions()
    {
        mxlRegions regions;
        if (mxlFabricsRegionsForFlowReader(_fr, &regions) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to get regions from flow data");
        }

        return MXLRegions{regions, RegionsDeleter{}};
    }

    MXLRegions FlowSetup::getCudaWriterRegions(std::uint64_t deviceId)
    {
        return getCudaRegions(deviceId, &_cudaWriterBuf);
    }

    MXLRegions FlowSetup::getCudaReaderRegions(std::uint64_t deviceId)
    {
        return getCudaRegions(deviceId, &_cudaReaderBuf);
    }

    MXLRegions makeCudaRegion(void* buf, std::size_t size, uint64_t deviceId)
    {
        mxlFabricsMemoryRegion region{
            reinterpret_cast<std::uintptr_t>(buf), size, {MXL_MEMORY_REGION_TYPE_CUDA, deviceId}
        };

        mxlRegions regions;
        mxlFabricsMemoryRegionGroup group{&region, 1};
        if (mxlFabricsRegionsFromBufferGroups(&group, 1, &regions))
        {
            throw std::runtime_error("failed to create mxl regions from cuda region groups");
        }

        return MXLRegions{regions, RegionsDeleter{}};
    }

    MXLRegions FlowSetup::getCudaRegions(std::uint64_t deviceId, void** cudaBuf)
    {
        if (*cudaBuf != nullptr)
        {
            return makeCudaRegion(*cudaBuf, _cudaBufSize, _cudaDevice);
        }

        mxl::lib::FlowParser descriptorParser{_flowConfig};
        _cudaBufSize = descriptorParser.getPayloadSize() + 8192; // 8192 is the header size.

        if (cudaSetDevice(deviceId) != cudaSuccess)
        {
            throw std::runtime_error("failed to select cuda device");
        }

        _cudaDevice = deviceId;
        if (cudaMalloc(cudaBuf, _cudaBufSize) != cudaSuccess)
        {
            throw std::runtime_error("failed to allocate cuda region");
        }

        return makeCudaRegion(*cudaBuf, _cudaBufSize, deviceId);
    }

    mxlInstance FlowSetup::instance() noexcept
    {
        return _mxl;
    }

    RateTimer FlowSetup::createRateTimer() noexcept
    {
        return {_flowInfo.discrete.grainRate.numerator, _flowInfo.discrete.grainRate.denominator};
    }

}
