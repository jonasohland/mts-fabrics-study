#include "Output.hpp"
#include <array>
#include <filesystem>
#include <stdexcept>
#include "CSV.hpp"

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
        writer.write_row(std::vector<std::string>{"RoundTrip", "TxTime", "RxTime"});

        // Append each samples
        for (size_t i = 0; i < nbSamples; i++)
        {
            writer.write_row(std::vector<std::string>{results[0][i], results[1][i], results[2][i]});
        }
    }

    void writeResults(std::string const& directory, Results results)
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
}
