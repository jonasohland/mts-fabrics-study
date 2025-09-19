#pragma once

#include <cstdint>
#include <string>
#include <thread>
#include <httplib.h>
#include <unordered_map>

namespace riedel::fabricsperf
{

    enum class PcmMetric
    {
        Pcie,
        Memory
    };

    std::string toString(PcmMetric metric);

    class Pcm
    {
    public:
        Pcm(std::string const& sockaddr);

        void run(PcmMetric, std::uint64_t nb_iter);

        // Already in CSV format
        std::unordered_map<PcmMetric, std::string> exportData();

    private:
        httplib::Client _client;

        std::unordered_map<PcmMetric, std::thread> _handles;
        std::unordered_map<PcmMetric, std::string> _data;
    };
}
