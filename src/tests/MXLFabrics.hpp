#pragma once

#include <chrono>
#include <thread>
#include <mxl/fabrics.h>
#include "../Defer.hpp"
#include "../StaticString.hpp"
#include "../Test.hpp"
#include "internal/Logging.hpp"
#include "mxl/mxl.h"
#include "Common.hpp"

namespace riedel::fabricsperf
{
    template<StaticString, TransferMode, PollMode, mxlFabricsProvider, mxlFabricsMemoryRegionType,
        mxlFabricsMemoryRegionType>
    class MXLFabrics;

    template<StaticString Name, TransferMode TM, PollMode Poll, mxlFabricsProvider Provider,
        mxlFabricsMemoryRegionType InitiatorLocation, mxlFabricsMemoryRegionType TargetLocation>
    class MXLFabricsFactory : public TestFactory
    {
    public:
        std::unique_ptr<Test> operator()() const final
        {
            return std::make_unique<
                MXLFabrics<Name, TM, Poll, Provider, InitiatorLocation, TargetLocation>>();
        }

        [[nodiscard]]
        constexpr std::string name() const final
        {
            return Name.value();
        }
    };

    template<StaticString Name, TransferMode TM, PollMode Poll, mxlFabricsProvider Provider,
        mxlFabricsMemoryRegionType InitiatorLocation, mxlFabricsMemoryRegionType TargetLocation>
    class MXLFabrics : public Test
    {
    public:
        using Factory =
            MXLFabricsFactory<Name, TM, Poll, Provider, InitiatorLocation, TargetLocation>;
        constexpr static int numWarmupIterations = 200;

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
        }

        void run(TestContext& ctx) override
        {
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

                if (i >= 0)
                {
                    ctx.timerStop(index);
                }

                ++index;
            }
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

        MxlRegions getReaderRegions(TestContext& ctx)
        {
            MxlRegions regions;
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

        MxlRegions getWriterRegions(TestContext& ctx)
        {
            MxlRegions regions;
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
    };
}
