#include "Pcm.hpp"
#include <thread>
#include <httplib.h>
#include <fmt/format.h>
#include <unordered_map>
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{
    std::string toString(PcmMetric metric)
    {
        switch (metric)
        {
            case PcmMetric::Pcie:   return "pcie";
            case PcmMetric::Memory: return "mem";
        }
    }

    Pcm::Pcm(std::string const& sockaddr)
        : _client(fmt::format("http://{}", sockaddr))
    {}

    void Pcm::run(PcmMetric metric, std::size_t nb_iter)
    {
        _data.clear();

        _handles[metric] = std::thread(
            [this, nb_iter, metric]()
            {
                auto url = fmt::format("/{}?nb_iter={}", toString(metric), nb_iter);
                MXL_INFO("GET url={}", url);

                auto result = _client.Get(url);
                if (auto err = result.error(); err != httplib::Error::Success)
                {
                    throw std::runtime_error(
                        fmt::format("failed to run metric request: {}", httplib::to_string(err)));
                }

                if (result->status != 200)
                {
                    throw std::runtime_error(
                        fmt::format("failed to run metric request: {}", result->body));
                }

                _data[metric] = result->body;
            });
    }

    std::unordered_map<PcmMetric, std::string> Pcm::exportData()
    {
        for (auto& [_, handle] : _handles)
        {
            handle.join();
        }

        return _data;
    }

}
