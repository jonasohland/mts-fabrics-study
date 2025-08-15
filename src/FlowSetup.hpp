#pragma once

#include <memory>
#include <string>
#include <mxl/fabrics.h>
#include <mxl/mxl.h>

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
        FlowSetup(std::string const& domain, std::string flowInfo);
        ~FlowSetup();

        MxlRegions getRegions();

    private:
        mxlInstance _mxl{nullptr};
        mxlFlowReader _fr{nullptr};
        mxlFlowWriter _fw{nullptr};

        FlowInfo _frInfo{};
        FlowInfo _fwInfo{};

        std::string _flowInfo;
    };
}
