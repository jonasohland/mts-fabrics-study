#pragma once

#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include "Pcm.hpp"

namespace riedel::fabricsperf
{
    using Results = std::vector<std::pair<std::string, std::array<std::vector<std::string>, 3>>>;
    using PerfCounters =
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>;
    using PcmData = std::unordered_map<std::string, std::unordered_map<PcmMetric, std::string>>;

    void writeResults(std::string const& filename, Results const& results);
    void writePerfCounters(std::string const& directory, PerfCounters const& counters);
    void writePcmData(std::string const& directory, PcmData const& counters);
}
