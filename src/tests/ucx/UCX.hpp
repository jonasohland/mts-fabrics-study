#pragma once

#include <chrono>
#include <algorithm>
#include <optional>
#include <string>
#include <cuda_runtime.h>
#include <system_error>
#include <ucp/api/ucp.h>
#include "../../ScopedGPUMaxClocks.hpp"
#include "../../StaticString.hpp"
#include "../../Test.hpp"
#include "../Common.hpp"
#include "internal/Logging.hpp"
#include "mxl/fabrics.h"
#include "Worker.hpp"

namespace riedel::fabricsperf
{
    template<StaticString Name, TransferMode TM, PollMode Poll, mxlFabricsMemoryRegionType SrcRegion,
        mxlFabricsMemoryRegionType DestRegion, ExtraCopyMode = ExtraCopyMode::NoExtraCopy>
    class UCXFactory;

    template<StaticString Name, TransferMode TM, PollMode Poll, mxlFabricsMemoryRegionType SrcRegion,
        mxlFabricsMemoryRegionType DestRegion, ExtraCopyMode ExtraCopy = ExtraCopyMode::NoExtraCopy>
    class UCX : public Test
    {
    public:
        using Factory = UCXFactory<Name, TM, Poll, SrcRegion, DestRegion, ExtraCopy>;

        constexpr static std::size_t const numWarmupIterations = 20;

        UCX() = default;

        static std::string hostname()
        {
            auto hostname = std::string(1024, '\0');
            auto namelen = ::gethostname(hostname.data(), hostname.size());
            if (namelen < 0)
            {
                throw std::system_error(errno, std::generic_category(), "gethostname");
            }

            hostname.resize(hostname.find('\0'));

            return hostname;
        }

        [[nodiscard]]
        bool needsGPU(TestContext const&) const noexcept
        {
            return SrcRegion == MXL_MEMORY_REGION_TYPE_CUDA ||
                   DestRegion == MXL_MEMORY_REGION_TYPE_CUDA ||
                   ExtraCopy == ExtraCopyMode::ExtraCopy;
        }

        MXLRegions getWriterRegions(TestContext& ctx)
        {
            MXLRegions regions;
            if constexpr (DestRegion == MXL_MEMORY_REGION_TYPE_HOST)
            {
                regions = ctx.flows().getWriterRegions();
            }
            else if constexpr (DestRegion == MXL_MEMORY_REGION_TYPE_CUDA)
            {
                regions = ctx.flows().getCudaWriterRegions(ctx.config().gpu.front());
            }
            else
            {
                static_assert(false, "Unsupported memory region location.");
            }

            return regions;
        }

        MXLRegions getReaderRegions(TestContext& ctx)
        {
            MXLRegions regions;
            if constexpr (SrcRegion == MXL_MEMORY_REGION_TYPE_HOST)
            {
                regions = ctx.flows().getReaderRegions();
            }
            else if constexpr (SrcRegion == MXL_MEMORY_REGION_TYPE_CUDA)
            {
                regions = ctx.flows().getCudaReaderRegions(ctx.config().gpu.front());
            }
            else
            {
                static_assert(false, "Unsupported memory region location.");
            }

            return regions;
        }

        void setup(TestContext& ctx) final
        {
            auto hn = hostname();

            _tx.emplace(fmt::format("{}-{}-tx", hn, ctx.runner() ? "runner" : "reflector"), false);
            _rx.emplace(fmt::format("{}-{}-rx", hn, ctx.runner() ? "runner" : "reflector"), false);

            _rx->listen(ctx.config().targetEndpoint);

            if (needsGPU(ctx))
            {
                _clockLocks.clear();
                for (auto const& id : ctx.config().gpu)
                {
                    _clockLocks.emplace_back(static_cast<int>(id));
                }
            }

            // register memory regions

            if (TM == TransferMode::Reflect || ctx.reflector())
            {
                MXL_INFO("register rx regions");
                auto writerRegions = getWriterRegions(ctx);
                _rxRegions = grainRegions(writerRegions);

                for (auto const& [buf, size, loc] : *_rxRegions)
                {
                    _rx->addLocalMemoryRegion(reinterpret_cast<void*>(buf),
                        size,
                        (loc.iface() == FI_HMEM_CUDA) ? MXL_MEMORY_REGION_TYPE_CUDA
                                                      : MXL_MEMORY_REGION_TYPE_HOST,
                        /* write */ true);

                    if constexpr (ExtraCopy == ExtraCopyMode::ExtraCopy)
                    {
                        if (auto status = cudaHostRegister(reinterpret_cast<void*>(buf), size, 0);
                            (status != cudaSuccess) &&
                            (status != cudaErrorHostMemoryAlreadyRegistered))
                        {
                            throw std::runtime_error(
                                fmt::format("failed to register host buffer to cuda device: {}",
                                    cudaGetErrorName(status)));
                        }
                    }
                }
            }

            if (TM == TransferMode::Reflect || ctx.runner())
            {
                MXL_INFO("register rx regions");
                auto readerRegions = getReaderRegions(ctx);
                _txRegions = grainRegions(readerRegions);

                for (auto const& [buf, size, loc] : *_txRegions)
                {
                    _tx->addLocalMemoryRegion(reinterpret_cast<void*>(buf),
                        size,
                        (loc.iface() == FI_HMEM_CUDA) ? MXL_MEMORY_REGION_TYPE_CUDA
                                                      : MXL_MEMORY_REGION_TYPE_HOST,
                        /* write */ false);

                    if constexpr (ExtraCopy == ExtraCopyMode::ExtraCopy)
                    {
                        if (auto status = cudaHostRegister(reinterpret_cast<void*>(buf), size, 0);
                            (status != cudaSuccess) &&
                            (status != cudaErrorHostMemoryAlreadyRegistered))
                        {
                            throw std::runtime_error(
                                fmt::format("failed to register host buffer to cuda device: {}",
                                    cudaGetErrorName(status)));
                        }
                    }
                }
            }

            if constexpr (ExtraCopy == ExtraCopyMode::ExtraCopy)
            {
                auto gpuMxlRegion = ctx.flows().getCudaWriterRegions(ctx.config().gpu.front());
                auto const& gpuRegions = grainRegions(gpuMxlRegion);
                if (gpuRegions.empty())
                {
                    throw std::runtime_error("no gpu regions allocated");
                }

                auto [buf, size, loc] = gpuRegions[0];
                _extraCopyBuf = reinterpret_cast<void*>(buf);
                _extraCopyBufSize = size;
            }

            ctx.setLocalTargetInfo(_rx->getTargetInfo());
        }

        void disconnect()
        {
            // only disconnect the tx side, the rx will be disconnected by the remote tx
            _tx->disconnect();

            while (_tx->isConnected() || _rx->isConnected())
            {
                _tx->makeProgressBlocking(std::chrono::milliseconds(100));
                _rx->makeProgress();
            }
        }

        void teardown(TestContext&) final
        {
            if (_extraCopyBuf)
            {
                cudaHostUnregister(_extraCopyBuf);
                _extraCopyBuf = nullptr;
            }

            _tx.reset();
            _rx.reset();

            _txRegions = std::nullopt;
            _rxRegions = std::nullopt;
        }

        void run(TestContext& ctx) final
        {
            connect(ctx);
            test(ctx);
            disconnect();
        }

        void transfer(TestContext& ctx, uint64_t index)
        {
            if (ExtraCopy == ExtraCopyMode::ExtraCopy)
            {
                auto [buf, size, loc] = (*_txRegions)[index % _txRegions->size()];
                if (auto status = ::cudaMemcpy(reinterpret_cast<void*>(buf),
                        _extraCopyBuf,
                        std::min(size, _extraCopyBufSize),
                        cudaMemcpyDeviceToHost);
                    status != cudaSuccess)
                {
                    throw std::runtime_error("extra copy failed");
                }
            }

            _tx->transferGrain(index);
            if constexpr (Poll == PollMode::SPIN)
            {
                while (_tx->makeProgress())
                {
                    if (ctx.interrupted() || !_rx->isConnected())
                    {
                        return;
                    }
                }
            }
            else if constexpr (Poll == PollMode::WAIT)
            {
                while (_tx->makeProgressBlocking(std::chrono::milliseconds(100)))
                {
                    if (ctx.interrupted() || !_rx->isConnected())
                    {
                        return;
                    }
                }
            }
        }

        std::optional<uint64_t> receive(TestContext& ctx)
        {
            std::optional<uint64_t> grain;
            if constexpr (Poll == PollMode::SPIN)
            {
                do
                {
                    if (ctx.interrupted())
                    {
                        return std::nullopt;
                    }

                    auto [xgrain, status] = _rx->receiveGrainNonBlocking();
                    if (status != UCS_OK)
                    {
                        MXL_ERROR("receive grain: {}", ::ucs_status_string(status));
                        return std::nullopt;
                    }

                    grain = xgrain;
                }
                while (!grain);
            }
            else if constexpr (Poll == PollMode::WAIT)
            {
                do
                {
                    if (!_rx->isConnected())
                    {
                        return std::nullopt;
                    }
                    if (ctx.interrupted())
                    {
                        return std::nullopt;
                    }

                    auto [xgrain,
                        status] = _rx->receiveGrainBlocking(std::chrono::milliseconds(100));
                    if (status != UCS_OK)
                    {
                        MXL_ERROR("receive grain: {}", ::ucs_status_string(status));
                        return std::nullopt;
                    }

                    grain = xgrain;
                }
                while (!grain);
            }

            if (grain)
            {
                if constexpr (ExtraCopy == ExtraCopyMode::ExtraCopy)
                {
                    auto [buf, size, loc] = (*_rxRegions)[*grain % _rxRegions->size()];
                    if (auto status = ::cudaMemcpy(_extraCopyBuf,
                            reinterpret_cast<void*>(buf),
                            std::min(size, _extraCopyBufSize),
                            cudaMemcpyHostToDevice);
                        status != cudaSuccess)
                    {
                        throw std::runtime_error("extra copy failed");
                    }
                }
            }

            return grain;
        }

        void connect(TestContext& ctx)
        {
            // wait for the remote target info
            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                if (_remoteTargetInfo)
                {
                    _tx->connect(*_remoteTargetInfo, ctx.config().initiatorEndpoint);
                    break;
                }
            }

            // establish connection on both rx and tx
            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                _tx->makeProgressBlocking(std::chrono::milliseconds(100));
                _rx->makeProgressBlocking(std::chrono::milliseconds(100));

                if (_tx->isConnected() && _rx->isConnected())
                {
                    break;
                }
            }
        }

        void testRunner(TestContext& ctx)
        {
            uint64_t index = 0;
            auto timer = ctx.flows().createRateTimer();

            for (int i = -static_cast<int>(numWarmupIterations);
                i < static_cast<int>(ctx.config().iterations);
                ++i)
            {
                if (!timer.waitUntilNextFrame())
                {
                    MXL_WARN("too slow");
                }

                if (i == 0)
                {
                    ctx.startPerfRecorder();
                    ctx.launchPcmPcieRecorder();
                    MXL_INFO("warmup complete");
                }

                if (i >= 0)
                {
                    if (TM == TransferMode::OneWay)
                    {
                        ctx.recordCurrentTime(index);
                    }
                    else
                    {
                        ctx.timerStart(index);
                    }
                }

                transfer(ctx, index);

                if (TM == TransferMode::OneWay)
                {
                    ++index;
                    continue;
                }

                auto receivedIndex = receive(ctx);
                if (!receivedIndex)
                {
                    return;
                }

                if (i >= 0)
                {
                    ctx.timerStop(index);
                }

                if (*receivedIndex != index)
                {
                    throw std::runtime_error("received wrong index");
                }

                ++index;
            }

            ctx.stopPerfRecorder();
        }

        void testReflector(TestContext& ctx)
        {
            uint64_t index = 0;
            uint64_t counter = 0;
            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                if (!_rx->isConnected())
                {
                    return;
                }

                auto grain = receive(ctx);
                if (!grain)
                {
                    return;
                }

                index = *grain;

                if (counter >= numWarmupIterations)
                {
                    ctx.recordCurrentTime(*grain);
                }

                if (TM == TransferMode::OneWay)
                {
                    counter++;
                    continue;
                }

                transfer(ctx, index);
                counter++;
            }
        }

        void test(TestContext& ctx)
        {
            MXL_INFO("tx and rx connected, moving on");
            if (ctx.reflector())
            {
                testReflector(ctx);
            }
            else
            {
                testRunner(ctx);
            }
        }

        void onRemoteEndpointAvailable(TestContext&, std::string info) final
        {
            _remoteTargetInfo = info;
        }

        bool needsReflector() const noexcept final
        {
            return true;
        }

    private:
        std::vector<ScopedGPUMaxClocks> _clockLocks{};

        std::optional<std::vector<MXLGrainRegion>> _rxRegions{std::nullopt};
        std::optional<std::vector<MXLGrainRegion>> _txRegions{std::nullopt};

        void* _extraCopyBuf;
        std::size_t _extraCopyBufSize;

        std::optional<std::string> _remoteTargetInfo;
        std::optional<UCPWorker> _tx;
        std::optional<UCPWorker> _rx;
    };

    template<StaticString Name, TransferMode TM, PollMode Poll, mxlFabricsMemoryRegionType SrcRegion,
        mxlFabricsMemoryRegionType DestRegion, ExtraCopyMode ExtraCopy>
    class UCXFactory : public TestFactory
    {
    public:
        std::string name() const final
        {
            return Name.value();
        }

        std::unique_ptr<Test> operator()() const final
        {
            return std::make_unique<UCX<Name, TM, Poll, SrcRegion, DestRegion, ExtraCopy>>();
        }
    };
}
