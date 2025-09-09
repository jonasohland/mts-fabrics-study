#include "Context.hpp"
#include <cstring>
#include <ucp/api/ucp.h>
#include "Status.hpp"

namespace riedel::fabricsperf
{
    UCPContext::UCPContext()
    {
        ::ucp_params_t params{};
        std::memset(&params, 0, sizeof(params));

        params.field_mask = UCP_PARAM_FIELD_FEATURES;
        params.features = UCP_FEATURE_RMA | UCP_FEATURE_WAKEUP | UCP_FEATURE_STREAM |
                          UCP_FEATURE_WAKEUP;

        ucx(::ucp_init, "create context", &params, nullptr, &_raw);
    }

    ::ucp_context_h UCPContext::raw() const noexcept
    {
        return _raw;
    }

    void UCPContext::registerMemoryRegion(void* address, std::size_t size,
        mxlFabricsMemoryRegionType region)
    {
        ::ucp_mem_map_params_t params = {
            .field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH |
                          UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE | UCP_MEM_MAP_PARAM_FIELD_PROT,
            .address = address,
            .length = size,
            .flags = 0,
            .prot = UCP_MEM_MAP_PROT_LOCAL_READ,
            .memory_type = (region == MXL_MEMORY_REGION_TYPE_CUDA) ? UCS_MEMORY_TYPE_CUDA
                                                                   : UCS_MEMORY_TYPE_HOST,
            .exported_memh_buffer = nullptr,
        };

        ::ucp_mem_h mem;
        ucx(::ucp_mem_map, "map memory region", _raw, &params, &mem);
        _regions.push_back(mem);
    }

    UCPContext::~UCPContext()
    {
        ::ucp_cleanup(_raw);
    }
}
