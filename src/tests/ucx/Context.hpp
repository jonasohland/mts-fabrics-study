#pragma once

#include <vector>
#include <mxl/fabrics.h>
#include <ucp/api/ucp.h>

namespace riedel::fabricsperf
{
    class UCPContext
    {
    public:
        UCPContext();
        ~UCPContext();

        ::ucp_context_h raw() const noexcept;

        void registerMemoryRegion(void* base, std::size_t size, mxlFabricsMemoryRegionType mrt);

    private:
        std::vector<::ucp_mem_h> _regions;
        ::ucp_rkey_h _rk;
        ::ucp_context_h _raw;
    };

}
