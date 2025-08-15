#include <CLI/CLI.hpp>
#include <mxl/mxl.h>
#include "internal/Logging.hpp"
#include "tests/MXLHost2Host.hpp"
#include "Executor.hpp"

namespace fp = riedel::fabricsperf;

std::function<void()>* stopHandler;

void signal_handler(int)
{
    (*stopHandler)();
}

int main(int argc, char** argv)
{
    CLI::App app{};

    fp::Config config = {
        .runner = false,
        .reflector = false,
        .listen = "",
        .connect = "",
        .test = "all",
        .node = "",
        .service = "",
        .output = "output.csv",
        .gpu = "0",
        .domain = "/dev/shm/mxl",
        .flow = "flow.json",
    };

    app.add_flag("--runner", config.runner, "Run as the test runner");
    app.add_flag("--reflector", config.reflector, "Run as the reflector instance");
    app.add_option("-n, --node", config.node, "Local fabric node address");
    app.add_option("-s, --service", config.service, "Local fabric service");
    app.add_option("-o, --output", config.output, "Path at which the report should be written");
    app.add_option("-g, --gpu", config.gpu, "Id of the gpu that should be used");
    app.add_option("-t, --test",
        config.test,
        "Name of the test to be run, use the special name 'all' to run all tests. Use 'list' to "
        "output a list of tests and exit");
    app.add_option(
        "-c, --connect", config.connect, "Address of the reflector to connect to (implies runner)");
    app.add_option("-f, --flow", config.flow, "NMOS flow configuration file");
    app.add_option("-l, --listen", config.listen, "Address to bind to (implies reflector)");

    CLI11_PARSE(app, argc, argv);

    std::signal(SIGINT, signal_handler);

    try
    {
        fp::Executor runner{std::move(config)};

        runner.add<fp::MXLHost2Host>();

        stopHandler = new std::function<void()>{
            [&]() { runner.stop(); },
        };

        runner.run();
    }
    catch (std::exception& ex)
    {
        MXL_ERROR("Failed to run test: {}", ex.what());
    }
}
