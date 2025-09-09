#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <mxl/fabrics.h>
#include <ucp/api/ucp.h>
#include "Context.hpp"

namespace riedel::fabricsperf
{
    class UCPWorker
    {
        struct LocalRegion
        {
            void* addr;
            std::size_t size;
            ::ucp_mem_h handle;
            ::ucs_memory_type_t memoryType;
            std::string rkey;
        };

        struct RemoteRegion
        {
            std::uintptr_t addr;
            std::size_t size;
            std::string rkeyp;
            ::ucp_rkey_h rkey;
        };

    public:
        UCPWorker(std::string name, bool useFence);
        ~UCPWorker();

        enum ConnectionFlags
        {
            WAIT_CONNECTED_SEND = (1 << 0),
            WAIT_CONNECTED_RECV = (1 << 1),
        };

        void listen(std::string);
        void connect(std::string const& targetInfo, std::string const& localAddress);
        void addLocalMemoryRegion(void*, std::size_t, mxlFabricsMemoryRegionType, bool);
        void disconnect();

        void transferGrain(uint64_t index);
        std::optional<uint64_t> receiveGrainNonBlocking();
        std::optional<uint64_t> receiveGrainBlocking(std::chrono::milliseconds);

        bool makeProgress();
        bool makeProgressBlocking(std::chrono::milliseconds duration);

        std::string getTargetInfo();
        bool isConnected() const noexcept;

    private:
        void postPutOp(uint64_t index);
        void postGrainIndexRecv();
        void postGrainIndexSend();

        bool hasWork() const noexcept;
        void afterProgress() noexcept;

        void beginHandshake();
        void handleConnection(::ucp_conn_request_h);
        void handleError(::ucp_ep_h, ::ucs_status_t);

        void importTargetInfo(std::string const&);
        void unpackRKeys();

        void destroyEndpoint();

        friend void ucpHandleError(void*, ::ucp_ep_h, ::ucs_status_t);
        friend void ucpHandleConnection(::ucp_conn_request_h, void*);

        UCPContext _ctx;

        std::optional<::ucp_listener_h> _listener{};
        std::optional<::ucp_ep_h> _endpoint{};
        std::optional<std::string> _bindAddress{};
        std::optional<std::string> _remoteAddress{};
        ::ucp_worker_h _raw{nullptr};
        int _efd{0};

        std::string _remoteNameBuffer;
        int _connectionWaitFlags{};

        bool _useFence = false;
        uint64_t _inFlightGrainIndex{0};
        std::size_t _recvLen;
        void* _recvRequest{nullptr};
        void* _sendRequest{nullptr};
        void* _putRequest{nullptr};
        void* _disconnectReq{nullptr};

        bool _rkeysReady{false};
        std::vector<LocalRegion> _localRegions;
        std::vector<RemoteRegion> _remoteRegions;

        std::string _name;
    };
}
