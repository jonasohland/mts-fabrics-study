#pragma once

#include <cuda_runtime.h>
#include <mxl/fabrics.h>
#include "../ScopedGPUMaxClocks.hpp"
#include "../StaticString.hpp"
#include "../Test.hpp"
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{

    template<StaticString Name, mxlFabricsMemoryRegionType InitiatorLocation,
        mxlFabricsMemoryRegionType TargetLocation>
    class NativeCuda;

    template<StaticString Name, mxlFabricsMemoryRegionType InitiatorLocation,
        mxlFabricsMemoryRegionType TargetLocation>
    struct NativeCudaFactory : public TestFactory
    {
        std::string name() const final
        {
            return Name.value();
        }

        std::unique_ptr<Test> operator()() const final
        {
            return std::make_unique<NativeCuda<Name, InitiatorLocation, TargetLocation>>();
        }
    };

    template<StaticString Name, mxlFabricsMemoryRegionType InitiatorLocation,
        mxlFabricsMemoryRegionType TargetLocation>
    class NativeCuda : public Test
    {
    public:
        using Factory = NativeCudaFactory<Name, InitiatorLocation, TargetLocation>;

        void onRemoteEndpointAvailable(TestContext&, std::string) final
        {}

        bool needsReflector() const noexcept final
        {
            return false;
        }

        void setup(TestContext& ctx) final
        {
            _cudaRegions = ctx.flows().getCudaWriterRegions(ctx.config().gpu);
            _hostRegions = ctx.flows().getWriterRegions();

            auto [cudaGrainPtr, cudaGrainSize, cuaLoc] = grainRegion(*_cudaRegions, 0);
            auto [hostGrainPtr, hostGrainSize, hostLoc] = grainRegion(*_hostRegions, 0);

            if (auto status = cudaHostRegister(
                    reinterpret_cast<void*>(hostGrainPtr), hostGrainSize, 0);
                status != cudaSuccess)
            {
                throw std::runtime_error(
                    fmt::format("cudaHostRegister: {}", cudaGetErrorName(status)));
            }

            _size = std::min(cudaGrainSize, hostGrainSize);

            if (InitiatorLocation == MXL_MEMORY_REGION_TYPE_CUDA &&
                TargetLocation == MXL_MEMORY_REGION_TYPE_HOST)
            {
                MXL_INFO("running host2device");
                _src = reinterpret_cast<void*>(cudaGrainPtr);
                _dst = reinterpret_cast<void*>(hostGrainPtr);
                _kind = cudaMemcpyDeviceToHost;
            }
            else if (InitiatorLocation == MXL_MEMORY_REGION_TYPE_HOST &&
                     TargetLocation == MXL_MEMORY_REGION_TYPE_CUDA)
            {
                MXL_INFO("running host2device");
                _src = reinterpret_cast<void*>(hostGrainPtr);
                _dst = reinterpret_cast<void*>(cudaGrainPtr);
                _kind = cudaMemcpyHostToDevice;
            }
            else
            {
                assert(false);
            }
        }

        void teardown(TestContext&) final
        {
            MXL_INFO("Teardown");
        }

        void run(TestContext& ctx) final
        {
            ScopedGPUMaxClocks clocks{static_cast<int>(ctx.config().gpu)};

            auto rate = ctx.flows().createRateTimer();
            auto index = 0;

            for (int i = -20; i < static_cast<int>(ctx.config().iterations); ++i)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                if (!rate.waitUntilNextFrame())
                {
                    MXL_WARN("failed to produce a grain in time");
                }
                if (i == 0)
                {
                    ctx.startPerfRecorder();
                    MXL_INFO("warmup complete");
                }

                if (i >= 0)
                {
                    ctx.timerStart(index);
                }

                if (auto status = cudaMemcpy(_src, _dst, _size, _kind); status != cudaSuccess)
                {
                    throw std::runtime_error(
                        fmt::format("cudaMemcpy: {}", cudaGetErrorName(status)));
                }

                if (i >= 0)
                {
                    ctx.timerStop(index);
                }

                ++index;
            }

            ctx.stopPerfRecorder();
        }

        void* _src;
        void* _dst;
        cudaMemcpyKind _kind;
        std::size_t _size;

        std::optional<MXLRegions> _cudaRegions;
        std::optional<MXLRegions> _hostRegions;

    private:
    };
}
