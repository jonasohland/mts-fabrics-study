#pragma once

#include <stdexcept>
#include <thread>
#include "../StaticString.hpp"
#include "../Test.hpp"
#include "internal/Logging.hpp"
#include "mxl/flow.h"
#include "Common.hpp"

namespace riedel::fabricsperf
{
    template<StaticString Name, TransferMode TM, PollMode PM>
    class MXLSHM;

    template<StaticString Name, TransferMode TM, PollMode PM>
    struct MXLSHMFactory : public TestFactory
    {
        std::string name() const final
        {
            return Name.value();
        }

        std::unique_ptr<Test> operator()() const final
        {
            return std::make_unique<MXLSHM<Name, TM, PM>>();
        }
    };

    template<StaticString Name, TransferMode TM, PollMode PM>
    class MXLSHM : public Test
    {
    public:
        using Factory = MXLSHMFactory<Name, TM, PM>;

        bool needsReflector() const noexcept final
        {
            return true;
        }

        void onRemoteEndpointAvailable(TestContext&, std::string info) final
        {
            _remoteFlowId = info;
        }

        void setup(TestContext& ctx) final
        {
            ctx.setLocalTargetInfo(uuids::to_string(ctx.flows().localFlowId()));
        }

        void teardown(TestContext&) final
        {}

        void run(TestContext& ctx) final
        {
            while (!_remoteFlowId)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (auto status = mxlCreateFlowReader(
                    ctx.flows().instance(), _remoteFlowId->c_str(), "", &_fr);
                status != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to initiatialize flow reader for remote flow");
            }

            ctx.signalReady();

            MXL_INFO("opened remote flow: {}", *_remoteFlowId);

            while (!ctx.remoteIsReady())
            {
                if (ctx.interrupted())
                {
                    return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (ctx.runner())
            {
                runner(ctx);
            }
            else if (ctx.reflector())
            {
                reflector(ctx);
            }
        }

    private:
        void runner(TestContext& ctx)
        {
            auto timer = ctx.flows().createRateTimer();
            auto writer = ctx.flows().writer();

            mxlGrainInfo grainInfo;
            uint8_t* pl;
            mxlStatus status;
            uint64_t index = 0;

            for (int i = -20; i < static_cast<int>(ctx.config().iterations); ++i)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                if (i == 0)
                {
                    MXL_INFO("Warmup complete");
                }

                if (!timer.waitUntilNextFrame())
                {
                    MXL_WARN("failed to produce a frame in-time");
                }

                if (i >= 0)
                {
                    if constexpr (TM == TransferMode::OneWay)
                    {
                        ctx.recordCurrentTime(index);
                    }
                    else
                    {
                        ctx.timerStart(index);
                    }
                }

                status = mxlFlowWriterOpenGrain(writer, index, &grainInfo, &pl);
                if (status != MXL_STATUS_OK)
                {
                    throw std::runtime_error("failed to open grain");
                }

                grainInfo.commitedSize = grainInfo.grainSize;

                status = mxlFlowWriterCommitGrain(writer, &grainInfo);
                if (status != MXL_STATUS_OK)
                {
                    throw std::runtime_error("failed to commit grain");
                }

                if (TM == TransferMode::OneWay)
                {
                    ++index;
                    continue;
                }

                if (!readGrainFromRemote(ctx, index))
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
            auto writer = ctx.flows().writer();

            mxlGrainInfo grainInfo;
            uint8_t* pl;
            mxlStatus status;
            uint64_t index = 0;

            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                if (!readGrainFromRemote(ctx, index))
                {
                    return;
                }

                if (index == 20)
                {
                    ctx.startPerfRecorder();
                }

                if (index >= 20)
                {
                    ctx.recordCurrentTime(index);
                }

                if (TM == TransferMode::OneWay)
                {
                    ++index;
                    continue;
                }

                status = mxlFlowWriterOpenGrain(writer, index, &grainInfo, &pl);
                if (status != MXL_STATUS_OK)
                {
                    throw std::runtime_error("failed to open grain");
                }

                grainInfo.commitedSize = grainInfo.grainSize;

                status = mxlFlowWriterCommitGrain(writer, &grainInfo);
                if (status != MXL_STATUS_OK)
                {
                    throw std::runtime_error("failed to commit grain");
                }

                ++index;
            }

            ctx.stopPerfRecorder();
        }

        uint64_t readGrainFromRemote(TestContext& ctx, uint64_t index)
        {
            mxlStatus status;
            mxlGrainInfo grainInfo;
            uint8_t* pl;

            if constexpr (PM == PollMode::SPIN)
            {
                for (;;)
                {
                    if (ctx.interrupted())
                    {
                        return false;
                    }

                    status = mxlFlowReaderGetGrainNonBlocking(_fr, index, &grainInfo, &pl);
                    if (status == MXL_ERR_OUT_OF_RANGE_TOO_EARLY)
                    {
                        continue;
                    }
                    if (status == MXL_STATUS_OK)
                    {
                        return true;
                    }

                    throw std::runtime_error(
                        fmt::format("failed to read grain: {}", static_cast<int>(status)));
                }
            }
            else
            {
                status = mxlFlowReaderGetGrain(_fr,
                    index,
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(5))
                        .count(),
                    &grainInfo,
                    &pl);
                if (status != MXL_STATUS_OK)
                {
                    throw std::runtime_error("failed to get grain");
                }
            }

            return true;
        }

        mxlFlowReader _fr;
        std::optional<std::string> _remoteFlowId;
    };

}
