#include "MXLHost2Host.hpp"
#include <stdexcept>
#include <thread>
#include <vector>
#include "../Defer.hpp"
#include "internal/Logging.hpp"
#include "mxl/fabrics.h"
#include "mxl/mxl.h"

namespace riedel::fabricsperf
{
    std::unique_ptr<Test> MXLHost2HostFactory::operator()() const
    {
        return std::make_unique<MXLHost2Host>();
    }

    std::string MXLHost2HostFactory::name() const
    {
        return "MXLHost2Host";
    }

    void MXLHost2Host::setup(TestContext& ctx)
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
        if (mxlFabricsTargetInfoToString(targetInfo, nullptr, &targetInfoStrSize) != MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to get target info string length");
        }

        std::vector<char> targetInfoBuf(targetInfoStrSize);
        if (mxlFabricsTargetInfoToString(targetInfo, targetInfoBuf.data(), &targetInfoStrSize) !=
            MXL_STATUS_OK)
        {
            throw std::runtime_error("failed to convert target info to string");
        }

        ctx.setLocalTargetInfo(std::string{targetInfoBuf.data(), targetInfoBuf.size() - 1});
    }

    void MXLHost2Host::teardown(TestContext&)
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

    void MXLHost2Host::run(TestContext& ctx)
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

    void MXLHost2Host::onRemoteEndpointAvailable(TestContext&, std::string info)
    {
        MXL_INFO("remote target available");
        _remoteEndpointInfo = info;
    }

    void MXLHost2Host::runner(TestContext& ctx)
    {
        mxlStatus status;
        uint64_t index = 0;
        for (;;)
        {
            status = mxlFabricsInitiatorTransferGrain(_in, index);
            if (status != MXL_STATUS_OK)
            {
                throw std::runtime_error("failed to submit grain to fabric");
            }

            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                ctx.timerStart(index);

                status = mxlFabricsInitiatorMakeProgressBlocking(_in, 10);
                if (status == MXL_ERR_NOT_READY)
                {
                    continue;
                }

                if (status == MXL_STATUS_OK)
                {
                    break;
                }

                throw std::runtime_error("initiator failed to make progress");
            }

            MXL_INFO("sent grain index: {}", index);

            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                status = mxlFabricsTargetWaitForNewGrain(_tg, &index, 10);
                if (status == MXL_ERR_TIMEOUT)
                {
                    continue;
                }

                if (status == MXL_STATUS_OK)
                {
                    ctx.timerStop();
                    break;
                }

                throw std::runtime_error("failed to receive grain");
            }

            MXL_INFO("received grain index: {}", index);
            ++index;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void MXLHost2Host::reflector(TestContext& ctx)
    {
        while (!ctx.interrupted())
        {
            uint64_t index;
            mxlStatus status;
            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                status = mxlFabricsTargetWaitForNewGrain(_tg, &index, 15);
                if (status == MXL_ERR_TIMEOUT)
                {
                    continue;
                }

                if (status == MXL_STATUS_OK)
                {
                    break;
                }

                throw std::runtime_error("something went wrong while receiving a grain");
            }

            status = mxlFabricsInitiatorTransferGrain(_in, index);
            if (status != MXL_STATUS_OK)
            {
                throw std::runtime_error(
                    "something went wrong while submitting received grain to fabric");
            }

            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                status = mxlFabricsInitiatorMakeProgressBlocking(_in, 15);
                if (status == MXL_ERR_NOT_READY)
                {
                    continue;
                }

                if (status == MXL_STATUS_OK)
                {
                    break;
                }

                throw std::runtime_error("something went wrong while transferring a grain");
            }

            MXL_INFO("reflected grain with index {}", index);
        }
    }
}
