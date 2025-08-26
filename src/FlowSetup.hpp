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
    namespace ofi = mxl::lib::fabrics::ofi;

    struct RegionsDeleter
    {
        void operator()(mxlRegions_t*) const;
    };

    using MxlRegions = std::unique_ptr<mxlRegions_t, RegionsDeleter>;

    std::tuple<std::uintptr_t, std::size_t, ofi::Region::Location> grainData(MxlRegions& regions,
        uint64_t index);

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

        MxlRegions getWriterRegions();
        MxlRegions getReaderRegions();

        MxlRegions getCudaWriterRegions(std::uint64_t deviceId);
        MxlRegions getCudaReaderRegions(std::uint64_t deviceId);

        mxlInstance instance() noexcept;
        RateTimer createRateTimer() noexcept;

    private:
        MxlRegions getCudaRegions(std::uint64_t deviceId, void** cudaBuf);

        mxlInstance _mxl{nullptr};
        mxlFlowReader _fr{nullptr};
        mxlFlowWriter _fw{nullptr};

        void* _cudaWriterBuf{nullptr};
        void* _cudaReaderBuf{nullptr};

        mxlFlowInfo _flowInfo{};
        std::string _flowConfig;
    };
}
