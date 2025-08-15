#include "Config.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <fmt/format.h>
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{
    Mode Config::mode() const
    {
        if (runner && reflector)
        {
            throw std::runtime_error("Can only run as runner or reflector, but not both.");
        }

        // guess by the provided addresses
        if (!runner && !reflector)
        {
            if (connect != "")
            {
                if (listen != "")
                {
                    throw std::runtime_error(
                        "No mode specified and both connect and listen "
                        "address provided, cannot guess which mode to run as.");
                }

                return Mode::RUNNER;
            }
            else if (listen != "")
            {
                return Mode::REFLECTOR;
            }
        }

        if (runner)
        {
            return Mode::RUNNER;
        }
        else
        {
            return Mode::REFLECTOR;
        }
    }

    int Config::listenPort() const
    {
        return std::stoi(listen.substr(listen.find(':') + 1, listen.size()));
    }

    std::string Config::listenHost() const
    {
        return listen.substr(0, listen.find(':'));
    }

    std::string Config::flowConfig() const
    {
        try
        {
            std::stringstream ss{};
            std::fstream fs{flow, std::fstream::in};

            ss << fs.rdbuf();

            auto str = ss.str();

            MXL_INFO("nmos flow def length: {}", str);

            return str;
        }
        catch (std::exception& ex)
        {
            throw std::runtime_error(fmt::format("read {}: {}", flow, ex.what()));
        }

        throw std::runtime_error(fmt::format("unknown error reading: {}", flow));
    }

}
