#pragma once

#include <chrono>
#include <string>
#include <system_error>
#include <ucp/api/ucp.h>
#include "../../Test.hpp"
#include "internal/Logging.hpp"
#include "Worker.hpp"

namespace riedel::fabricsperf
{
    class UCXFactory;

    class UCX : public Test
    {
    public:
        using Factory = UCXFactory;

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

        void setup(TestContext& ctx) final
        {
            auto hn = hostname();

            _tx.emplace(fmt::format("{}-{}-tx", hn, ctx.runner() ? "runner" : "reflector"), false);
            _rx.emplace(fmt::format("{}-{}-rx", hn, ctx.runner() ? "runner" : "reflector"), false);

            _rx->listen(ctx.config().targetEndpoint);

            // register memory regions
            auto writerRegions = ctx.flows().getWriterRegions();
            auto readerRegions = ctx.flows().getReaderRegions();

            for (auto const& [buf, size, loc] : grainRegions(writerRegions))
            {
                _rx->addLocalMemoryRegion(reinterpret_cast<void*>(buf),
                    size,
                    MXL_MEMORY_REGION_TYPE_HOST,
                    /* write */ true);
            }

            for (auto const& [buf, size, loc] : grainRegions(readerRegions))
            {
                _tx->addLocalMemoryRegion(reinterpret_cast<void*>(buf),
                    size,
                    MXL_MEMORY_REGION_TYPE_HOST,
                    /* write */ false);
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
            _tx.reset();
            _rx.reset();
        }

        void run(TestContext& ctx) final
        {
            connect(ctx);
            test(ctx);
            disconnect();
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

        void test(TestContext& ctx)
        {
            MXL_INFO("tx and rx connected, moving on");

            uint64_t index = 0;
            auto rate = ctx.flows().createRateTimer();

            for (;;)
            {
                if (ctx.runner())
                {
                    if (!rate.waitUntilNextFrame())
                    {
                        MXL_INFO("too slow");
                    }
                }
                if (ctx.interrupted())
                {
                    return;
                }
                if (!_rx->isConnected())
                {
                    MXL_INFO("rx no longer connected");
                    return;
                }

                if (ctx.runner())
                {
                    _tx->transferGrain(index);
                    while (_tx->makeProgressBlocking(std::chrono::milliseconds(100)))
                    {
                        if (ctx.interrupted())
                        {
                            return;
                        }
                    }

                    ++index;
                }
                else
                {
                    for (;;)
                    {
                        if (ctx.interrupted())
                        {
                            return;
                        }

                        if (!_rx->isConnected())
                        {
                            MXL_INFO("rx no longer connected, stopping");
                            return;
                        }

                        auto grain = _rx->receiveGrainBlocking(std::chrono::milliseconds(100));
                        if (!grain)
                        {
                            continue;
                        }

                        index = *grain;
                        MXL_INFO("received: {}", index);
                    }
                }
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
        std::optional<std::string> _remoteTargetInfo;
        std::optional<UCPWorker> _tx;
        std::optional<UCPWorker> _rx;
    };

    class UCXFactory : public TestFactory
    {
    public:
        std::string name() const final
        {
            return "UCX";
        }

        std::unique_ptr<Test> operator()() const final
        {
            return std::make_unique<UCX>();
        }
    };
}
