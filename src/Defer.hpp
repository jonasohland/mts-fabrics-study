#pragma once

#include <utility>

namespace riedel::fabricsperf
{

    template<typename F>
    struct Deferred
    {
        ~Deferred()
        {
            f();
        }

        F f;
    };

    template<typename F>
    Deferred<F> defer(F f)
    {
        return Deferred<F>{std::forward<F>(f)};
    }

}
