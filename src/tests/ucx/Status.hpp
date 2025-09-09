#pragma once

#include <stdexcept>
#include <utility>
#include <fmt/format.h>
#include <ucp/api/ucp.h>
#include <ucs/type/status.h>

namespace riedel::fabricsperf
{
    template<typename F, typename S, typename... Args>
    void ucx(F fun, S msg, Args&&... args)
    {
        auto status = fun(std::forward<Args>(args)...);
        if (status != UCS_OK)
        {
            throw std::runtime_error(
                fmt::format("ucx: {}: {}", std::forward<S>(msg), ucs_status_string(status)));
        }
    }

    template<typename F, typename S, typename... Args>
    void* ucxReq(F fun, S msg, Args&&... args)
    {
        auto ptr = fun(std::forward<Args>(args)...);
        if (UCS_PTR_IS_ERR(ptr) && UCS_PTR_STATUS(ptr) != UCS_INPROGRESS)
        {
            throw std::runtime_error(fmt::format(
                "ucx: {}: {}", std::forward<S>(msg), ucs_status_string(UCS_PTR_STATUS(ptr))));
        }

        return ptr;
    }
}
