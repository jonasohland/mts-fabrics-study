#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>
#include <cuda_runtime.h>
#include <fmt/format.h>
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
            std::vector<std::uintptr_t> cudaGrain;
            for (size_t i = 0; i < ctx.config().gpu.size(); i++)
            {
                _cudaRegions.push_back(ctx.flows().getCudaWriterRegions(ctx.config().gpu[i]));
                auto [cudaGrainPtr, cudaGrainSize, cudaLoc] = grainRegion(_cudaRegions[i], 0);
                cudaGrain.push_back(cudaGrainPtr);
                _size = std::min(_size, cudaGrainSize);
            }

            _hostRegions = ctx.flows().getWriterRegions();
            auto [hostGrainPtr, hostGrainSize, hostLoc] = grainRegion(*_hostRegions, 0);

            if (auto status = cudaHostRegister(
                    reinterpret_cast<void*>(hostGrainPtr), hostGrainSize, 0);
                status != cudaSuccess)
            {
                throw std::runtime_error(
                    fmt::format("cudaHostRegister: {}", cudaGetErrorName(status)));
            }

            _size = std::min(_size, hostGrainSize);

            if (InitiatorLocation == MXL_MEMORY_REGION_TYPE_CUDA &&
                TargetLocation == MXL_MEMORY_REGION_TYPE_HOST)
            {
                MXL_INFO("running device2host");
                _src = reinterpret_cast<void*>(cudaGrain.front());
                _dst = reinterpret_cast<void*>(hostGrainPtr);
                _kind = cudaMemcpyDeviceToHost;
            }
            else if (InitiatorLocation == MXL_MEMORY_REGION_TYPE_HOST &&
                     TargetLocation == MXL_MEMORY_REGION_TYPE_CUDA)
            {
                MXL_INFO("running host2device");
                _src = reinterpret_cast<void*>(hostGrainPtr);
                _dst = reinterpret_cast<void*>(cudaGrain.front());
                _kind = cudaMemcpyHostToDevice;
            }
            else if (InitiatorLocation == MXL_MEMORY_REGION_TYPE_CUDA &&
                     TargetLocation == MXL_MEMORY_REGION_TYPE_CUDA)
            {
                // Use peer API
                if (ctx.config().gpu.size() == 2 && ctx.config().gpu[0] != ctx.config().gpu[1])
                {
                    enableCudaPeerAccess(ctx.config().gpu[0], ctx.config().gpu[1]);
                }

                MXL_INFO("running device2device");
                _src = reinterpret_cast<void*>(cudaGrain[0]);
                _dst = reinterpret_cast<void*>(cudaGrain[1]);
                _kind = cudaMemcpyDeviceToDevice;
            }
            else
            {
                assert(false);
            }
        }

        void teardown(TestContext& ctx) final
        {
            MXL_INFO("Teardown");

            if (InitiatorLocation == MXL_MEMORY_REGION_TYPE_CUDA &&
                TargetLocation == MXL_MEMORY_REGION_TYPE_CUDA && ctx.config().gpu.size() == 2 &&
                ctx.config().gpu[0] != ctx.config().gpu[1])
            {
                disableCudaPeerAccess(ctx.config().gpu[0], ctx.config().gpu[1]);
            }
        }

        void run(TestContext& ctx) final
        {
            std::vector<ScopedGPUMaxClocks> clocks;
            for (auto const& gpuId : ctx.config().gpu)
            {
                clocks.emplace_back(ScopedGPUMaxClocks{static_cast<int>(gpuId)});
            }

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
                    ctx.launchPcmPcieRecorder();
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

                if (_kind == cudaMemcpyDeviceToDevice)
                {
                    if (auto status = cudaDeviceSynchronize(); status != cudaSuccess)
                    {
                        throw std::runtime_error(
                            fmt::format("cudaDeviceSynchronize: {}", cudaGetErrorName(status)));
                    }
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
        std::size_t _size = std::numeric_limits<size_t>::max();

        std::vector<MXLRegions> _cudaRegions;
        std::optional<MXLRegions> _hostRegions;

    private:
        void enableCudaPeerAccess(int gpu1Id, int gpu2Id)
        {
            int canAccess = 0;
            if (auto status = cudaDeviceCanAccessPeer(&canAccess, gpu1Id, gpu2Id);
                status != cudaSuccess)
            {
                throw std::runtime_error(
                    fmt::format("cudaDeviceCanAccessPeer: {}", cudaGetErrorName(status)));
            }

            if (!canAccess)
            {
                throw std::runtime_error(fmt::format(
                    "cuda peer api not available between gpu \"{}\" and \"{}\"", gpu1Id, gpu2Id));
            }

            if (auto status = cudaSetDevice(gpu1Id); status != cudaSuccess)
            {
                throw std::runtime_error(
                    fmt::format("cudaSetDevice: {}", cudaGetErrorName(status)));
            }

            if (auto status = cudaDeviceEnablePeerAccess(gpu2Id, 0);
                status != cudaSuccess && status != cudaErrorPeerAccessAlreadyEnabled)
            {
                throw std::runtime_error(
                    fmt::format("cudaDeviceEnablePeerAccess: {}", cudaGetErrorName(status)));
            }

            if (auto status = cudaSetDevice(gpu2Id); status != cudaSuccess)
            {
                throw std::runtime_error(
                    fmt::format("cudaSetDevice: {}", cudaGetErrorName(status)));
            }
            if (auto status = cudaDeviceEnablePeerAccess(gpu1Id, 0);
                status != cudaSuccess && status != cudaErrorPeerAccessAlreadyEnabled)
            {
                throw std::runtime_error(
                    fmt::format("cudaDeviceEnablePeerAccess: {}", cudaGetErrorName(status)));
            }
        }

        void disableCudaPeerAccess(int gpu1Id, int gpu2Id)
        {
            if (auto status = cudaSetDevice(gpu1Id); status == cudaSuccess)
            {
                if (auto status = cudaDeviceDisablePeerAccess(gpu2Id))
                {
                    MXL_ERROR("cudaDeviceDisablePeerAccess: {}", cudaGetErrorName(status));
                }
            }

            if (auto status = cudaSetDevice(gpu2Id); status == cudaSuccess)
            {
                if (auto status = cudaDeviceDisablePeerAccess(gpu1Id))
                {
                    MXL_ERROR("cudaDeviceDisablePeerAccess: {}", cudaGetErrorName(status));
                }
            }
        }
    };
}
