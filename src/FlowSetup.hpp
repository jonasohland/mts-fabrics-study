#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <uuid.h>
#include <mxl/fabrics.h>
#include <mxl/flow.h>
#include <mxl/mxl.h>
#include "Rate.hpp"
#include "Region.hpp"

namespace riedel::fabricsperf
{
#ifdef MXL_FABRICS_NATIVE
    namespace fabrics = mxl::lib::fabrics::rdma_core;
#elif MXL_FABRICS_OFI
    namespace fabrics = mxl::lib::fabrics::ofi;
#endif

    struct RegionsDeleter
    {
        void operator()(mxlRegions_t*) const;
    };

    using MXLRegions = std::unique_ptr<mxlRegions_t, RegionsDeleter>;

    using MXLGrainRegion = std::tuple<std::uintptr_t, std::size_t, fabrics::Region::Location>;

    MXLGrainRegion grainRegion(MXLRegions const& regions, uint64_t index);
    std::vector<MXLGrainRegion> grainRegions(MXLRegions const& regions);

    class FlowSetup
    {
    public:
        FlowSetup(std::string const& domain, std::string flowConfig);
        ~FlowSetup();

        void destroy();

        uuids::uuid localFlowId() noexcept;

        mxlFlowInfo const& flowInfo() const noexcept;

        mxlFlowReader reader() noexcept;
        mxlFlowWriter writer() noexcept;

        MXLRegions getWriterRegions();
        MXLRegions getReaderRegions();

        MXLRegions getCudaWriterRegions(std::uint64_t deviceId);
        MXLRegions getCudaReaderRegions(std::uint64_t deviceId);

        mxlInstance instance() noexcept;
        RateTimer createRateTimer() noexcept;

    private:
        MXLRegions getCudaRegions(std::uint64_t deviceId, void** cudaBuf);

        mxlInstance _mxl{nullptr};
        mxlFlowReader _fr{nullptr};
        mxlFlowWriter _fw{nullptr};

        uint64_t _cudaDevice = 0;
        std::size_t _cudaBufSize = 0;
        void* _cudaWriterBuf{nullptr};
        void* _cudaReaderBuf{nullptr};

        mxlFlowInfo _flowInfo{};
        std::string _flowConfig;
    };
}
