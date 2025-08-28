#pragma once

#include <chrono>
#include <thread>
#include <cuda_runtime.h>
#include <mxl/fabrics.h>
#include "../Defer.hpp"
#include "../ScopedGPUMaxClocks.hpp"
#include "../StaticString.hpp"
#include "../Test.hpp"
#include "internal/Logging.hpp"
#include "mxl/mxl.h"
#include "Common.hpp"

namespace riedel::fabricsperf
{
    enum class ExtraCopyMode
    {
        NoExtraCopy,
        ExtraCopy
    };

    template<StaticString, TransferMode, PollMode, mxlFabricsProvider, mxlFabricsMemoryRegionType,
        mxlFabricsMemoryRegionType, ExtraCopyMode extraCopy = ExtraCopyMode::NoExtraCopy>
    class MXLFabrics;

    template<StaticString Name, TransferMode TM, PollMode Poll, mxlFabricsProvider Provider,
        mxlFabricsMemoryRegionType InitiatorLocation, mxlFabricsMemoryRegionType TargetLocation,
        ExtraCopyMode ExtraCopy = ExtraCopyMode::NoExtraCopy>
    class MXLFabricsFactory : public TestFactory
    {
    public:
        std::unique_ptr<Test> operator()() const final
        {
            return std::make_unique<
                MXLFabrics<Name, TM, Poll, Provider, InitiatorLocation, TargetLocation, ExtraCopy>>();
        }

        [[nodiscard]]
        constexpr std::string name() const final
        {
            return Name.value();
        }
    };

    template<StaticString Name, TransferMode TM, PollMode Poll, mxlFabricsProvider Provider,
        mxlFabricsMemoryRegionType InitiatorLocation, mxlFabricsMemoryRegionType TargetLocation,
        ExtraCopyMode ExtraCopy>
    class MXLFabrics : public Test
    {
    public:
        using Factory = MXLFabricsFactory<Name, TM, Poll, Provider, InitiatorLocation,
            TargetLocation, ExtraCopy>;
        constexpr static int numWarmupIterations = 200;

        [[nodiscard]]
        bool needsGPU(TestContext const& ctx) const noexcept
        {
            return (InitiatorLocation == MXL_MEMORY_REGION_TYPE_CUDA && ctx.runner()) ||
                   (TargetLocation == MXL_MEMORY_REGION_TYPE_CUDA && ctx.reflector());
        }

        [[nodiscard]]
        bool needsReflector() const noexcept final
        {
            return true;
        }

        bool runInitiator(TestContext const& ctx)
        {
            mxlStatus status;
            for (;;)
            {
                if (ctx.interrupted())
                {
                    return false;
                }

                if constexpr (Poll == PollMode::WAIT)
                {
                    status = mxlFabricsInitiatorMakeProgressBlocking(_in, 100);
                }
                else
                {
                    status = mxlFabricsInitiatorMakeProgressNonBlocking(_in);
                }

                if (status == MXL_ERR_TIMEOUT)
                {
                    continue;
                }
                if (status == MXL_ERR_NOT_READY)
                {
                    continue;
                }
                if (status == MXL_STATUS_OK)
                {
                    return true;
                }

                throw std::runtime_error(fmt::format("initiator failed to make progress: code {}",
                    static_cast<int>(status)));
            }
        }

        bool runTarget(TestContext const& ctx, uint64_t* index)
        {
            mxlStatus status;

            for (;;)
            {
                if (ctx.interrupted())
                {
                    return false;
                }

                // Poll with wait
                if constexpr (Poll == PollMode::WAIT)
                {
                    status = mxlFabricsTargetWaitForNewGrain(_tg, index, 100);
                }
                else // non-blocking poll
                {
                    status = mxlFabricsTargetTryNewGrain(_tg, index);
                }

                if (status == MXL_ERR_TIMEOUT)
                {
                    continue;
                }
                if (status == MXL_ERR_NOT_READY)
                {
                    continue;
                }
                if (status == MXL_STATUS_OK)
                {
                    return true;
                }

                throw std::runtime_error(fmt::format("failed to get grain from target: code {}",
                    static_cast<int>(status)));
            }

            return index;
        }

        void doExtraTransferAfter(uint64_t index)
        {
            auto const& [buf, size, log] =
                (*_localGrainRegions)[index % _localGrainRegions->size()];

            auto status = cudaMemcpy(_extraBuf,
                reinterpret_cast<void*>(buf),
                std::max(_extraBufSize, size),
                cudaMemcpyHostToDevice);

            if (status != cudaSuccess)
            {
                throw std::runtime_error(
                    fmt::format("failed to copy grain to device: {}", cudaGetErrorName(status)));
            }
        }

        void doExtraTransferBefore(uint64_t index)
        {
            auto const& [buf, size, log] =
                (*_localGrainRegions)[index % _localGrainRegions->size()];

            auto status = cudaMemcpy(reinterpret_cast<void*>(buf),
                _extraBuf,
                std::max(_extraBufSize, size),
                cudaMemcpyDeviceToHost);

            if (status != cudaSuccess)
            {
                throw std::runtime_error(
                    fmt::format("failed to copy grain from device: {}", cudaGetErrorName(status)));
            }
        }

        void setup(TestContext& ctx) override
        {
            if (mxlFabricsCreateInstance(ctx.flows().instance(), &_instance) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to create farbrics instance");
            }

            if (mxlFabricsCreateInitiator(_instance, &_in) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to create initiator");
            }

            if (mxlFabricsCreateTarget(_instance, &_tg) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to create target");
            }

            auto targetEndpointNode = ctx.config().targetEndpointNode();
            auto targetEndpointService = ctx.config().targetEndpointService();
            auto initiatorEndpointNode = ctx.config().initiatorEndpointNode();
            auto initiatorEndpointService = ctx.config().initiatorEndpointService();
            auto writerRegions = getWriterRegions(ctx);
            auto readerRegions = getReaderRegions(ctx);

            // clang-format off
            auto initiatorConfig = mxlInitiatorConfig{
                .endpointAddress = mxlEndpointAddress{
                   .node = initiatorEndpointNode.c_str(),
                   .service = initiatorEndpointService.c_str(),
                },
                .provider = Provider,
                .regions = readerRegions.get(),
                .deviceSupport=InitiatorLocation != MXL_MEMORY_REGION_TYPE_HOST,
            };

            auto targetConfig = mxlTargetConfig{
                .endpointAddress = mxlEndpointAddress{
                   .node = targetEndpointNode.c_str(),
                   .service = targetEndpointService.c_str(),
                 },
                .provider = Provider,
                .regions = writerRegions.get(),
                .deviceSupport=TargetLocation != MXL_MEMORY_REGION_TYPE_HOST,
            };
            // clang-format on

            if (mxlFabricsInitiatorSetup(_in, &initiatorConfig) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to set up initiator");
            }

            mxlTargetInfo targetInfo;

            if (mxlFabricsTargetSetup(_tg, &targetConfig, &targetInfo) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to set up target");
            }

            auto _ = defer([targetInfo]() { mxlFabricsFreeTargetInfo(targetInfo); });

            std::size_t targetInfoStrSize;
            if (mxlFabricsTargetInfoToString(targetInfo, nullptr, &targetInfoStrSize) !=
                MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to get target info string length");
            }

            std::vector<char> targetInfoBuf(targetInfoStrSize);
            if (mxlFabricsTargetInfoToString(
                    targetInfo, targetInfoBuf.data(), &targetInfoStrSize) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to convert target info to string");
            }

            ctx.setLocalTargetInfo(std::string{targetInfoBuf.data(), targetInfoBuf.size() - 1});

            if constexpr (ExtraCopy == ExtraCopyMode::ExtraCopy)
            {
                auto regions = ctx.flows().getWriterRegions();
                _localGrainRegions = grainRegions(regions);

                for (auto const& region : *_localGrainRegions)
                {
                    auto const& [buf, size, loc] = region;

                    MXL_INFO(
                        "register host grain region to device: [at 0x{:x}, size: {}]", buf, size);

                    if (auto status = cudaHostRegister(reinterpret_cast<void*>(buf), size, 0);
                        status != cudaSuccess)
                    {
                        throw std::runtime_error(fmt::format(
                            "failed to register host grain region: {}", cudaGetErrorName(status)));
                    }
                }

                auto extraRegions = ctx.flows().getCudaWriterRegions(ctx.config().gpu);
                auto [buf, size, loc] = grainRegion(extraRegions, 0);

                assert(loc.iface() == FI_HMEM_CUDA);

                _extraBuf = reinterpret_cast<void*>(buf);
                _extraBufSize = size;
            }
        }

        void teardown(TestContext&) override
        {
            if (_tg)
            {
                if (mxlFabricsDestroyTarget(_instance, _tg) != MXL_STATUS_OK)
                {
                    MXL_ERROR("Failed to destroy target");
                }
            }

            if (_tg)
            {
                if (mxlFabricsDestroyInitiator(_instance, _in) != MXL_STATUS_OK)
                {
                    MXL_ERROR("Failed to destroy initiator");
                }
            }

            if (_instance)
            {
                if (mxlFabricsDestroyInstance(_instance) != MXL_STATUS_OK)
                {
                    MXL_ERROR("Failed to destroy instance");
                }
            }

            if (_localGrainRegions)
            {
                for (auto const& region : *_localGrainRegions)
                {
                    auto const& [buf, size, loc] = region;
                    if (auto status = cudaHostUnregister(reinterpret_cast<void*>(buf));
                        status != cudaSuccess)
                    {
                        MXL_ERROR("failed to unregister host memory region from device: {}",
                            cudaGetErrorName(status));
                    }
                }
            }
        }

        void run(TestContext& ctx) override
        {
            std::optional<ScopedGPUMaxClocks> gpuClocksLock;
            if (needsGPU(ctx))
            {
                gpuClocksLock.emplace(static_cast<int>(ctx.config().gpu));
            }

            while (!_remoteEndpointInfo)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            mxlTargetInfo targetInfo;
            if (mxlFabricsTargetInfoFromString(_remoteEndpointInfo->c_str(), &targetInfo) !=
                MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to parse target info");
            }

            auto _ = defer([targetInfo]() { mxlFabricsFreeTargetInfo(targetInfo); });

            if (mxlFabricsInitiatorAddTarget(_in, targetInfo) != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to add target to initiator");
            }

            bool initiatorReady = false;

            // Wait until both this and the remote initiator have connected
            while (!initiatorReady || !ctx.remoteIsReady())
            {
                if (ctx.interrupted())
                {
                    return;
                }

                if (!initiatorReady)
                {
                    auto status = mxlFabricsInitiatorMakeProgressBlocking(_in, 100);
                    // done adding target
                    if (status == MXL_STATUS_OK)
                    {
                        initiatorReady = true;
                        ctx.signalReady();
                    }
                }

                // We also need to poll the target, so the remote initiator can connect
                uint64_t index;
                auto status = mxlFabricsTargetWaitForNewGrain(_tg, &index, 100);
                if (status == MXL_ERR_TIMEOUT || status == MXL_STATUS_OK)
                {
                    continue;
                }

                // something went wrong
                throw std::runtime_error("an error ocurred while trying to set up the initiator");
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));

            if (ctx.runner())
            {
                return runner(ctx);
            }

            else if (ctx.reflector())
            {
                return reflector(ctx);
            }
        }

        void onRemoteEndpointAvailable(TestContext&, std::string info) override
        {
            MXL_INFO("remote target available");
            _remoteEndpointInfo = info;
        }

    private:
        void runner(TestContext& ctx)
        {
            auto timer = ctx.flows().createRateTimer();

            mxlStatus status;
            uint64_t index = 0;

            for (int i = -numWarmupIterations; i < static_cast<int>(ctx.config().iterations); i++)
            {
                if (!timer.waitUntilNextFrame())
                {
                    MXL_WARN("failed to produce a frame in-time");
                    --i;
                    continue;
                }

                if (i == 0)
                {
                    MXL_INFO("Warmup complete");
                    ctx.startPerfRecorder();
                }

                if (i >= 0)
                {
                    if constexpr (TM == TransferMode::Reflect)
                    {
                        ctx.timerStart(index);
                    }
                    else
                    {
                        ctx.recordCurrentTime(index);
                    }
                }

                if constexpr (ExtraCopy == ExtraCopyMode::ExtraCopy)
                {
                    doExtraTransferBefore(index);
                }

                for (;;)
                {
                    status = mxlFabricsInitiatorTransferGrain(_in, index);
                    if (status == MXL_ERR_NOT_READY)
                    {
                        if (ctx.interrupted())
                        {
                            return;
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                    if (status != MXL_STATUS_OK)
                    {
                        throw std::runtime_error("failed to submit grain to fabric");
                    }

                    break;
                }

                if (!runInitiator(ctx))
                {
                    return;
                }

                // Only wait for reflected grain when running reflect transfer mode.
                if (TM == TransferMode::OneWay)
                {
                    ++index;
                    continue;
                }

                if (!runTarget(ctx, &index))
                {
                    return;
                }

                if constexpr (ExtraCopy == ExtraCopyMode::ExtraCopy)
                {
                    doExtraTransferAfter(index);
                }

                if (i >= 0)
                {
                    ctx.timerStop(index);
                }

                ++index;
            }

            ctx.stopPerfRecorder();
        }

        void reflector(TestContext& ctx)
        {
            uint64_t counter = 0;
            while (!ctx.interrupted())
            {
                uint64_t index;
                mxlStatus status;

                if (!runTarget(ctx, &index))
                {
                    return;
                }

                if constexpr (ExtraCopy == ExtraCopyMode::ExtraCopy)
                {
                    doExtraTransferAfter(index);
                }

                if (counter >= numWarmupIterations)
                {
                    ctx.recordCurrentTime(index);
                }

                // Only reflect back when we are running reflect transfer mode
                if constexpr (TM == TransferMode::OneWay)
                {
                    ++counter;
                    continue;
                }

                if constexpr (ExtraCopy == ExtraCopyMode::ExtraCopy)
                {
                    doExtraTransferBefore(index);
                }

                for (;;)
                {
                    status = mxlFabricsInitiatorTransferGrain(_in, index);
                    if (status == MXL_ERR_NOT_READY)
                    {
                        if (ctx.interrupted())
                        {
                            return;
                        }

                        // this should only happen for the first transfer while we are still in
                        // warmup mode
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                    if (status != MXL_STATUS_OK)
                    {
                        throw std::runtime_error(
                            "something went wrong while submitting received grain to fabric");
                    }

                    break;
                }

                if (!runInitiator(ctx))
                {
                    return;
                }

                ++counter;
            }
        }

        MXLRegions getReaderRegions(TestContext& ctx)
        {
            MXLRegions regions;
            if constexpr (InitiatorLocation == MXL_MEMORY_REGION_TYPE_HOST)
            {
                regions = ctx.flows().getReaderRegions();
            }
            else if constexpr (InitiatorLocation == MXL_MEMORY_REGION_TYPE_CUDA)
            {
                regions = ctx.flows().getCudaReaderRegions(ctx.config().gpu);
            }
            else
            {
                static_assert(false, "Unsupported memory region location.");
            }

            return regions;
        }

        MXLRegions getWriterRegions(TestContext& ctx)
        {
            MXLRegions regions;
            if constexpr (TargetLocation == MXL_MEMORY_REGION_TYPE_HOST)
            {
                regions = ctx.flows().getWriterRegions();
            }
            else if constexpr (TargetLocation == MXL_MEMORY_REGION_TYPE_CUDA)
            {
                regions = ctx.flows().getCudaWriterRegions(ctx.config().gpu);
            }
            else
            {
                static_assert(false, "Unsupported memory region location.");
            }

            return regions;
        }

        mxlFabricsInstance _instance;
        mxlFabricsInitiator _in;
        mxlFabricsTarget _tg;
        std::optional<RateTimer> _rt;
        std::optional<std::string> _remoteEndpointInfo;

        // Only used when there is an extra simulated cuda copy
        std::optional<std::vector<MXLGrainRegion>> _localGrainRegions;
        std::size_t _extraBufSize;
        void* _extraBuf;
    };
}
