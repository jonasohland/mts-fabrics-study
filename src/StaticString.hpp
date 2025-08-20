#pragma once

#include <cstddef>
#include <algorithm>
#include <array>
#include <string>

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

        [[nodiscard]]
        constexpr std::string value() const
        {
            return {data.data(), data.size() - 1};
        }
    };

    template<std::size_t N>
    StaticString(char const (&)[N]) -> StaticString<N - 1>; // NOLINT

}
