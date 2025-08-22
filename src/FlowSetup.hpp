#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <mxl/fabrics.h>
#include <mxl/mxl.h>
#include "Rate.hpp"

namespace riedel::fabricsperf
{
    struct RegionsDeleter
    {
        void operator()(mxlRegions_t*) const;
    };

    using MxlRegions = std::unique_ptr<mxlRegions_t, RegionsDeleter>;

    class FlowSetup
    {
    public:
        FlowSetup(std::string const& domain, std::string flowConfig);
        ~FlowSetup();

        void destroy();

        MxlRegions getWriterRegions();
        MxlRegions getReaderRegions();

        MxlRegions getCudaWriterRegions(std::uint64_t deviceId);
        MxlRegions getCudaReaderRegions(std::uint64_t deviceId);
        mxlInstance instance();
        RateTimer createRateTimer();

    private:
        MxlRegions getCudaRegions(std::uint64_t deviceId, void** cudaBuf);

        mxlInstance _mxl{nullptr};
        mxlFlowReader _fr{nullptr};
        mxlFlowWriter _fw{nullptr};

        void* _cudaWriterBuf{nullptr};
        void* _cudaReaderBuf{nullptr};

        FlowInfo _flowInfo{};
        std::string _flowConfig;
    };
}
