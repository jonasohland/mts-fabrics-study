#include <CLI/CLI.hpp>
#include <mxl/mxl.h>
#include "internal/Logging.hpp"
#include "mxl/fabrics.h"
#include "tests/MXLFabrics.hpp"
#include "tests/MXLSHM.hpp"
#include "tests/NativeCuda.hpp"
#include "tests/TestTemplateOneWay.hpp"
#include "tests/TestTemplatePingPong.hpp"
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
        .runner = true,
        .reflector = false,
        .listen = "",
        .connect = "",
        .run = {},
        .targetEndpoint = "127.0.0.1:9992",
        .initiatorEndpoint = "127.0.0.1:9993",
        .output = "output/",
        .gpu = 0,
        .domain = "/dev/shm/mxl",
        .flow = "flow.json",
        .iterations = 2000,
    };

    app.add_flag("--runner", config.runner, "Run as the test runner");
    app.add_flag("--reflector", config.reflector, "Run as the reflector instance");
    app.add_option("-t, --target", config.targetEndpoint, "Local fabric node address");
    app.add_option("-i, --initiator", config.initiatorEndpoint, "Local fabric service");
    app.add_option("-o, --output",
        config.output,
        "Directory at which the report should be written. Each test case will output a csv file "
        "with the name of the test case. Ex: <output>/MXLFabrics+Host2Host+Verbs+Reflect+Wait.csv");
    app.add_option("-g, --gpu", config.gpu, "Id of the gpu that should be used");
    app.add_option("-r, --run",
        config.run,
        "Name of the test to be run, use the special name 'all' to run all tests. Use 'list' to "
        "output a list of tests and exit");
    app.add_option(
        "-c, --connect", config.connect, "Address of the reflector to connect to (implies runner)");
    app.add_option("-f, --flow", config.flow, "NMOS flow configuration file");
    app.add_option("-n, --iterations", config.iterations, "Number of test iterations");
    app.add_option("-l, --listen", config.listen, "Address to bind to (implies reflector)");
    app.add_option("-d, --domain", config.domain, "MXL Domain");

    CLI11_PARSE(app, argc, argv);

    std::signal(SIGINT, signal_handler);

    try
    {
        fp::Executor runner{std::move(config)};

        // clang-format off
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+Verbs+Reflect+Wait", fp::TransferMode::Reflect, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_VERBS, MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+Verbs+Reflect+Spin", fp::TransferMode::Reflect, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_VERBS,MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+Verbs+OneWay+Wait", fp::TransferMode::OneWay, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_VERBS,MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+Verbs+OneWay+Spin", fp::TransferMode::OneWay, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_VERBS,MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+TCP+Reflect+Wait", fp::TransferMode::Reflect, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_TCP,MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+TCP+Reflect+Spin", fp::TransferMode::Reflect, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_TCP,MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+TCP+OneWay+Wait", fp::TransferMode::OneWay, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_TCP,MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+TCP+OneWay+Spin", fp::TransferMode::OneWay, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_TCP,MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Cuda+Verbs+Oneway+Wait", fp::TransferMode::OneWay, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_VERBS, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Cuda+Verbs+Oneway+Spin", fp::TransferMode::OneWay, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_VERBS, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Cuda+Verbs+Reflect+Wait", fp::TransferMode::Reflect, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_VERBS, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Cuda+Verbs+Reflect+Spin", fp::TransferMode::Reflect, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_VERBS, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Cuda+SHM+Oneway+Wait", fp::TransferMode::OneWay, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Cuda+SHM+Oneway+Spin", fp::TransferMode::OneWay, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Host+SHM+Oneway+Wait", fp::TransferMode::OneWay, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Host+SHM+Oneway+Spin", fp::TransferMode::OneWay, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Cuda+SHM+Oneway+Wait", fp::TransferMode::OneWay, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Cuda+SHM+Oneway+Spin", fp::TransferMode::OneWay, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Cuda+SHM+Reflect+Wait", fp::TransferMode::Reflect, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Cuda2Cuda+SHM+Reflect+Spin", fp::TransferMode::Reflect, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+SHM+OneWay+Wait", fp::TransferMode::OneWay, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+SHM+OneWay+Spin", fp::TransferMode::OneWay, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+SHM+Reflect+Wait", fp::TransferMode::Reflect, fp::PollMode::WAIT, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLFabrics<"MXLFabrics+Host2Host+SHM+Reflect+Spin", fp::TransferMode::Reflect, fp::PollMode::SPIN, MXL_SHARING_PROVIDER_SHM, MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::MXLSHM<"MXLSHM+Reflect+Spin", fp::TransferMode::Reflect, fp::PollMode::SPIN>>();
        runner.add<fp::MXLSHM<"MXLSHM+Reflect+Wait", fp::TransferMode::Reflect, fp::PollMode::WAIT>>();
        runner.add<fp::MXLSHM<"MXLSHM+OneWay+Spin", fp::TransferMode::OneWay, fp::PollMode::SPIN>>();
        runner.add<fp::MXLSHM<"MXLSHM+OneWay+Wait", fp::TransferMode::OneWay, fp::PollMode::WAIT>>();
        runner.add<fp::NativeCuda<"NativeCuda+Host2Device", MXL_MEMORY_REGION_TYPE_HOST, MXL_MEMORY_REGION_TYPE_CUDA>>();
        runner.add<fp::NativeCuda<"NativeCuda+Device2Host", MXL_MEMORY_REGION_TYPE_CUDA, MXL_MEMORY_REGION_TYPE_HOST>>();
        runner.add<fp::OneWayTest>();
        runner.add<fp::PingPongTest>();
        //clang-format on

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
