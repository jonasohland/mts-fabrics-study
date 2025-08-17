#pragma once

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
        mxlInstance instance();
        RateTimer createRateTimer();

    private:
        mxlInstance _mxl{nullptr};
        mxlFlowReader _fr{nullptr};
        mxlFlowWriter _fw{nullptr};

        FlowInfo _flowInfo{};
        std::string _flowConfig;
    };
}
