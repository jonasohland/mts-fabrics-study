#include "Output.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <fmt/format.h>
#include <unordered_map>
#include "CSV.hpp"
#include "Pcm.hpp"

namespace riedel::fabricsperf
{
    void writeTestResults(csv::Writer& writer,
        std::array<std::vector<std::string>, 3> const& results)
    {
        size_t nbSamples = results[0].size();

        // Verify that we have the same amount of samples per axis
        for (auto const& result : results)
        {
            if (result.size() != nbSamples)
            {
                throw std::runtime_error("Result axis are all supposed to be of the same size.");
            }
        }

        // Append the CSV header
        writer.write_row(std::vector<std::string>{"Timers", "TxTime", "RxTime"});

        // Append each samples
        for (size_t i = 0; i < nbSamples; i++)
        {
            writer.write_row(std::vector<std::string>{results[0][i], results[1][i], results[2][i]});
        }
    }

    void writeResults(std::string const& directory, Results const& results)
    {
        std::filesystem::create_directories(directory);

        for (auto const& [testName, testResults] : results)
        {
            auto outputFile = directory + "/" + testName + ".csv";

            std::ofstream ofs{outputFile};
            csv::Writer csvw{ofs};

            writeTestResults(csvw, testResults);
        }
    }

    void writePerfCounter(std::string const& directory, std::string const& testName,
        std::vector<std::pair<std::string, std::string>> const& counters)
    {
        std::filesystem::create_directories(directory);
        std::ofstream ofs{directory + "/" + testName + ".perf.csv"};
        csv::Writer csvw{ofs};

        std::vector<std::string> titles{};
        std::vector<std::string> values{};

        std::transform(counters.begin(),
            counters.end(),
            std::back_inserter(titles),
            [](auto const& item) { return item.first; });
        std::transform(counters.begin(),
            counters.end(),
            std::back_inserter(values),
            [](auto const& item) { return item.second; });

        csvw.write_row(titles);
        csvw.write_row(values);
    }

    void writePerfCounters(std::string const& directory, PerfCounters const& counters)
    {
        for (auto const& [name, counters] : counters)
        {
            writePerfCounter(directory, name, counters);
        }
    }

    void writeNvmlPcieCounterTest(std::string const& directory, std::string const& testName,
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> const&
            devices)
    {
        std::vector<std::string> titles = {"tx_throughput", "rx_throughput"};
        for (auto const& [deviceId, samples] : devices)
        {
            std::ofstream ofs{directory + "/" + testName + "_" + deviceId + ".pcie.csv"};
            csv::Writer csvw{ofs};

            csvw.write_row(titles);
            for (auto const& sample : samples)
            {
                csvw.write_row(std::vector{sample.first, sample.second});
            }
        }
    }

    void writeNvmlPcieCounters(std::string const& directory, PcieCounters const& counters)
    {
        std::filesystem::create_directories(directory);

        for (auto const& [testName, devices] : counters)
        {
            writeNvmlPcieCounterTest(directory, testName, devices);
        }
    }

    void writePcmData(std::string const& directory, PcmData const& counters)
    {
        std::filesystem::create_directories(directory);

        for (auto const& [testName, test] : counters)
        {
            for (auto const& [pcm, data] : test)
            {
                auto fileName = fmt::format("{}/{}.pcm.{}.csv", directory, testName, toString(pcm));
                std::ofstream ofs{fileName};

                ofs << data;
            }
        }
    }
}
