#pragma once
#include <cassert>
#include <chrono>
#include <memory>
#include <thread>
#include "../Test.hpp"
#include "internal/Logging.hpp"

namespace riedel::fabricsperf
{
    class PingPongTestFactory;

    class PingPongTest : public Test
    {
    public:
        using Factory = PingPongTestFactory;

        void setup(TestContext& ctx) final
        {
            MXL_INFO("Setting up test.");
            // Create resources, allocate device memory, etc

            // Access to the memory regions of a pre-configured reader.
            auto _rr = ctx.flows().getReaderRegions();

            // Access to the memory regions of a pre-configured writer.
            auto _wr = ctx.flows().getWriterRegions();

            // Setting up something
            std::this_thread::sleep_for(std::chrono::milliseconds(::rand() % 1000));

            if (ctx.runner())
            {
                auto address = "127.0.0.1:2222";
                MXL_INFO("Local address is: {}", address);
                ctx.setLocalTargetInfo(address);
            }
            else
            {
                auto address = "127.0.0.1:4444";
                MXL_INFO("Local address is: {}", address);
                ctx.setLocalTargetInfo(address);
            }
        }

        void teardown(TestContext&) final
        {
            MXL_INFO("Tearing down test.");
            // Release resources, free device memory, etc
        }

        void onRemoteEndpointAvailable(TestContext&, std::string info) final
        {
            MXL_INFO("Remote endpoint information is now available");
            _remoteEndpointInfo = info;
        }

        void run(TestContext& ctx) final
        {
            // Establish connection, wait for both peers to be ready and run the test

            // Waiting for the remote endpoint to finish its setup
            while (!_remoteEndpointInfo)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Establish the connection.
            connect(*_remoteEndpointInfo);

            // Signal that we are ready to do the work.
            ctx.signalReady();

            // Wait for the remote side to also call ctx.signalReady()
            while (!ctx.remoteIsReady())
            {
                if (ctx.interrupted())
                {
                    return;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            MXL_INFO("Test running...");

            if (ctx.runner())
            {
                // This is is the "runner" side, that initiates
                // the grain transfers and measures the RTT
                runAsTestRunner(ctx);
            }
            else if (ctx.reflector())
            {
                // This is the "reflector" side, that waits for incoming grains
                // and sends them back immediately
                runAsReflector(ctx);
            }
            else
            {
                assert(false);
            }
        }

    private:
        void runAsTestRunner(TestContext& ctx)
        {
            uint64_t index = 0;

            // Create a timer that ticks in constant intervals
            // of the frame time defined in the flow.
            auto timer = ctx.flows().createRateTimer();

            for (uint64_t i = 0; i < ctx.config().iterations; ++i)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                if (!timer.waitUntilNextFrame())
                {
                    MXL_ERROR("Too late!");
                }

                // Start the timer right before sending a grain.
                ctx.timerStart(index);

                // Send the grain to the remote peer.
                sendGrain(index);

                // Wait for remote peer to send back the grain.
                receiveGrain(&index);

                // Stop the timer.
                ctx.timerStop(index);

                // Increment the grain index.
                ++index;
            }
        }

        void runAsReflector(TestContext& ctx)
        {
            uint64_t index = 0;

            for (;;)
            {
                if (ctx.interrupted())
                {
                    return;
                }

                // Receive a grain.
                receiveGrain(&index);

                // Record the time of the received grain.
                ctx.recordCurrentTime(index);

                // Send back the received grain.
                sendGrain(index);
            }
        }

        void connect(std::string remoteEndpointInfo)
        {
            MXL_INFO("Connecting to: {}", remoteEndpointInfo);

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        void sendGrain(uint64_t index)
        {
            // Send a grain to the other peer
            auto _ = index;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        void receiveGrain(uint64_t* index)
        {
            // Receive a grain from the other peer
            *index = 0;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::optional<std::string> _remoteEndpointInfo;
    };

    class PingPongTestFactory : public TestFactory
    {
    public:
        [[nodiscard]]
        std::string name() const final
        {
            return "PingPongTestTemplate";
        }

        std::unique_ptr<Test> operator()() const final
        {
            return std::make_unique<PingPongTest>();
        }
    };

}
