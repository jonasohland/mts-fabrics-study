#pragma once

#include <array>
#include <string>
#include <vector>
#include <unordered_map>

namespace riedel::fabricsperf
{
    using Results = std::vector<std::pair<std::string, std::array<std::vector<std::string>, 3>>>;
    using PerfCounters =
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>;

    using PcieCounters = std::unordered_map<std::string,
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>>;

    void writeResults(std::string const& filename, Results const& results);
    void writePerfCounters(std::string const& directory, PerfCounters const& counters);
    void writeNvmlPcieCounters(std::string const& directory, PcieCounters const& counters);
}
