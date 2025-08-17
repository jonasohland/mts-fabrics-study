#pragma once

#include <cstddef>
#include <algorithm>
#include <array>

namespace riedel::fabricsperf
{

    template<std::size_t N>
    struct StaticString
    {
        std::array<char, N + 1> data = {};

        constexpr StaticString(char const (&input)[N + 1]) // NOLINT
        {
            std::ranges::copy_n(input, N + 1, data.begin());
        }
    };

    template<std::size_t N>
    StaticString(char const (&)[N]) -> StaticString<N - 1>; // NOLINT

}
