#pragma once

#include <array>
#include <string>
#include <vector>

namespace riedel::fabricsperf
{
    using Results = std::vector<std::pair<std::string, std::array<std::vector<std::string>, 3>>>;

    void writeResults(std::string const& filename, Results results);
}
