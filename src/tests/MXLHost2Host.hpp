#include <mxl/fabrics.h>
#include "../Defer.hpp"
#include "../StaticString.hpp"
#include "../Test.hpp"
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{
    enum class PollMode
    {
        WAIT,
        SPIN
    };

    enum class TransferMode
    {
        Reflect,
        OneWay
    };

    template<StaticString, TransferMode, PollMode>
    class MXLHost2Host;

    template<StaticString Name, TransferMode TM, PollMode Poll>
    class MXLHost2HostFactory : public TestFactory
    {
    public:
        std::unique_ptr<Test> operator()() const final
        {
            return std::make_unique<MXLHost2Host<Name, TM, Poll>>();
        }

        [[nodiscard]]
        constexpr std::string name() const final
        {
            return Name.data.data();
        }
    };

    template<StaticString Name, TransferMode TM, PollMode Poll>
    class MXLHost2Host : public Test
    {
    public:
        using Factory = MXLHost2HostFactory<Name, TM, Poll>;

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

            auto readerRegions = ctx.flows().getReaderRegions();
            auto writerRegions = ctx.flows().getWriterRegions();
            auto targetEndpointNode = ctx.config().targetEndpointNode();
            auto targetEndpointService = ctx.config().targetEndpointService();
            auto initiatorEndpointNode = ctx.config().initiatorEndpointNode();
            auto initiatorEndpointService = ctx.config().initiatorEndpointService();

            // clang-format off
            auto initiatorConfig = mxlInitiatorConfig{
                .endpointAddress = mxlEndpointAddress{
                   .node = initiatorEndpointNode.c_str(),
                   .service = initiatorEndpointService.c_str(),
                },
                .provider = MXL_SHARING_PROVIDER_TCP,
                .regions = readerRegions.get(),
            };

            auto targetConfig = mxlTargetConfig{
                .endpointAddress = mxlEndpointAddress{
                   .node = targetEndpointNode.c_str(),
                   .service = targetEndpointService.c_str(),
                 },
                .provider = MXL_SHARING_PROVIDER_TCP,
                .regions = writerRegions.get(),
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

            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                auto status = mxlFabricsInitiatorMakeProgressBlocking(_in, 10);

                // done adding target
                if (status == MXL_STATUS_OK)
                {
                    break;
                }

                uint64_t index;
                status = mxlFabricsTargetWaitForNewGrain(_tg, &index, 10);
                if (status == MXL_ERR_TIMEOUT || status == MXL_STATUS_OK)
                {
                    continue;
                }

                // something went wrong
                throw std::runtime_error("an error ocurred while trying to set up the initiator");
            }

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
            for (;;)
            {
                if (!timer.waitUntilNextFrame())
                {
                    MXL_WARN("failed to produce a frame in-time");
                    continue;
                }

                status = mxlFabricsInitiatorTransferGrain(_in, index);
                if (status != MXL_STATUS_OK)
                {
                    throw std::runtime_error("failed to submit grain to fabric");
                }

                if (!runInitiator(ctx))
                {
                    return;
                }

                // Only wait for reflected grain when running reflect transfer mode.
                if (TM == TransferMode::OneWay)
                {
                    continue;
                }

                if (!runTarget(ctx, &index))
                {
                    return;
                }

                ++index;
            }
        }

        void reflector(TestContext& ctx)
        {
            while (!ctx.interrupted())
            {
                uint64_t index;
                mxlStatus status;

                if (!runTarget(ctx, &index))
                {
                    return;
                }

                // Only reflect back when we are running reflect transfer mode
                if constexpr (TM == TransferMode::OneWay)
                {
                    continue;
                }

                status = mxlFabricsInitiatorTransferGrain(_in, index);
                if (status != MXL_STATUS_OK)
                {
                    throw std::runtime_error(
                        "something went wrong while submitting received grain to fabric");
                }

                if (!runInitiator(ctx))
                {
                    return;
                }
            }
        }

        mxlFabricsInstance _instance;
        mxlFabricsInitiator _in;
        mxlFabricsTarget _tg;
        std::optional<RateTimer> _rt;
        std::optional<std::string> _remoteEndpointInfo;
    };
}
