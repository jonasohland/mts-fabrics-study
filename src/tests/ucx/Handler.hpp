#pragma once

#include <utility>

namespace riedel::fabricsperf
{
    template<typename H, typename T>
    H ucxHandler(T* thiz, decltype(std::declval<H>().cb) cb)
    {
        return H{
            .cb = cb,
            .arg = thiz,
        };
    }
}
