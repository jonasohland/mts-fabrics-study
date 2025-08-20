#include "Output.hpp"
#include "CSV.hpp"

namespace riedel::fabricsperf
{
    void writeTestResultRow(csv::Writer& writer, std::string const& name, std::string_view kind,
        std::vector<std::string> const& results)
    {
        std::vector<std::string> row{results.size() + 2};
        row[0] = name;
        row[1] = kind;

        std::copy(results.begin(), results.end(), row.begin() + 2);

        writer.write_row(row);
    }

    void writeTestResults(csv::Writer& writer, std::string const& name,
        std::array<std::vector<std::string>, 3> const& results)
    {
        writeTestResultRow(writer, name, "RoundTrip", results[0]);
        writeTestResultRow(writer, name, "TxTime", results[1]);
        writeTestResultRow(writer, name, "RxTime", results[2]);
    }

    void writeResults(std::string const& filename, Results results)
    {
        std::ofstream ofs{filename};
        csv::Writer csvw{ofs};

        for (auto const& [testName, testResults] : results)
        {
            writeTestResults(csvw, testName, testResults);
        }
    }
}
