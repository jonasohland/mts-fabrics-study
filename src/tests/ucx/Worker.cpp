#include "Worker.hpp"
#include <stdexcept>
#include <poll.h>
#include <arpa/inet.h>
#include <picojson/picojson.h>
#include <ucp/api/ucp.h>
#include <ucp/api/ucp_compat.h>
#include <ucp/api/ucp_def.h>
#include <ucs/memory/memory_type.h>
#include "internal/Logging.hpp"
#include "Base64.hpp"
#include "Handler.hpp"
#include "Status.hpp"

namespace riedel::fabricsperf
{
    ::sockaddr_in parseSockaddr(std::string s)
    {
        auto ipaddr = s.substr(0, s.find(':'));
        auto service = s.substr(s.find(':') + 1, s.size());
        auto port = std::stoi(service);

        ::sockaddr_in inaddr = {
            .sin_family = AF_INET,
            .sin_port = static_cast<uint16_t>(port),
            .sin_addr = {},
            .sin_zero = {},
        };

        std::memset(&inaddr.sin_zero, 0, sizeof(inaddr.sin_zero));

        if (::inet_pton(AF_INET, ipaddr.c_str(), &inaddr.sin_addr) <= 0)
        {
            throw std::system_error(errno, std::generic_category(), "parse ip address");
        }

        return inaddr;
    }

    void ucpHandleError(void* arg, ucp_ep_h ep, ucs_status_t status)
    {
        reinterpret_cast<UCPWorker*>(arg)->handleError(ep, status);
    }

    void ucpHandleConnection(ucp_conn_request_h conn, void* arg)
    {
        reinterpret_cast<UCPWorker*>(arg)->handleConnection(conn);
    }

    UCPWorker::UCPWorker(std::string name, bool useFence)
        : _ctx()
        , _useFence(useFence)
        , _name(std::move(name))
    {
        ::ucp_worker_params_t param{};
        param.name = _name.c_str();
        param.thread_mode = UCS_THREAD_MODE_SINGLE;
        param.field_mask = UCP_WORKER_PARAM_FIELD_NAME | UCP_WORKER_PARAM_FIELD_THREAD_MODE;

        std::memset(&param.cpu_mask.ucs_bits, 0, sizeof(param.cpu_mask.ucs_bits));

        ucx(::ucp_worker_create, "create worker", _ctx.raw(), &param, &_raw);
        ucx(::ucp_worker_get_efd, "get ucp worker efd", _raw, &_efd);
    }

    UCPWorker::~UCPWorker()
    {
        if (_listener)
        {
            ::ucp_listener_destroy(*_listener);
        }

        ::ucp_worker_destroy(_raw);

        for (auto& reg : _localRegions)
        {
            ::ucp_mem_unmap(_ctx.raw(), reg.handle);
        }
    }

    void UCPWorker::addLocalMemoryRegion(void* addr, std::size_t size,
        mxlFabricsMemoryRegionType regionType, bool write)
    {
        std::string rkey;
        ::ucp_mem_map_params_t params;
        ::ucp_mem_h memh;

        params.field_mask = UCP_MEM_MAP_PARAM_FIELD_FLAGS | UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
                            UCP_MEM_MAP_PARAM_FIELD_LENGTH | UCP_MEM_MAP_PARAM_FIELD_MEMORY_TYPE |
                            UCP_MEM_MAP_PARAM_FIELD_PROT;
        params.flags = 0;
        params.address = addr;
        params.length = size;
        params.memory_type = (regionType == MXL_MEMORY_REGION_TYPE_HOST) ? UCS_MEMORY_TYPE_HOST
                                                                         : UCS_MEMORY_TYPE_CUDA;
        params.prot = UCP_MEM_MAP_PROT_LOCAL_READ | (write ? UCP_MEM_MAP_PROT_REMOTE_WRITE : 0);

        MXL_INFO("{}: adding local region 0x{:x} - 0x{:x}, type: {}",
            _name,
            reinterpret_cast<std::uintptr_t>(params.address),
            reinterpret_cast<std::uintptr_t>(params.address) + params.length,
            (params.memory_type == UCS_MEMORY_TYPE_CUDA) ? "cuda" : "host");

        // do the registration
        ucx(::ucp_mem_map, "register memory", _ctx.raw(), &params, &memh);

        // if this a region that will be written to, we need to export the rkey
        if (write)
        {
            char* buf;
            std::size_t size;
            ::ucp_memh_pack_params_t packParams{};
            packParams.field_mask = 0;
            ucx(::ucp_rkey_pack,
                "pack rkey",
                _ctx.raw(),
                memh,
                reinterpret_cast<void**>(&buf),
                &size);

            rkey = base64::to_base64(std::string{buf, size});

            ::ucp_rkey_buffer_release(buf);
        }

        _localRegions.emplace_back(LocalRegion{addr, size, memh, params.memory_type, rkey});
    }

    void UCPWorker::listen(std::string addr)
    {
        if (_listener)
        {
            throw std::runtime_error("listener already created");
        }

        _bindAddress = addr;

        auto sockaddr = parseSockaddr(std::move(addr));

        ::ucp_listener_h listener;

        ::ucp_listener_params params{};
        params.field_mask = UCP_LISTENER_PARAM_FIELD_CONN_HANDLER |
                            UCP_LISTENER_PARAM_FIELD_SOCK_ADDR;
        params.conn_handler = ucxHandler<::ucp_listener_conn_handler_t>(this, ucpHandleConnection);
        params.sockaddr.addr = reinterpret_cast<::sockaddr const*>(&sockaddr);
        params.sockaddr.addrlen = sizeof(::sockaddr);

        ucx(::ucp_listener_create, "create listener", _raw, &params, &listener);
        _listener.emplace(listener);
    }

    void UCPWorker::connect(std::string const& targetInfo, std::string const& localAddress)
    {
        if (_endpoint)
        {
            throw std::runtime_error("endpoint already created");
        }

        importTargetInfo(targetInfo);

        ::ucp_ep_h endpoint{};

        auto const localSockAddress = parseSockaddr(localAddress);
        auto const remoteSockAddress = parseSockaddr(*_remoteAddress);

        ::ucp_ep_params_t params{};
        params.field_mask = UCP_EP_PARAM_FIELD_SOCK_ADDR | UCP_EP_PARAM_FIELD_NAME |
                            UCP_EP_PARAM_FIELD_ERR_HANDLER | UCP_EP_PARAM_FIELD_FLAGS;
        params.name = _name.c_str();
        params.flags = UCP_EP_PARAMS_FLAGS_CLIENT_SERVER;
        params.err_handler = ucxHandler<::ucp_err_handler_t>(this, ucpHandleError);
        params.local_sockaddr.addr = reinterpret_cast<::sockaddr const*>(&localSockAddress);
        params.local_sockaddr.addrlen = sizeof(localAddress);
        params.sockaddr.addr = reinterpret_cast<::sockaddr const*>(&remoteSockAddress);
        params.sockaddr.addrlen = sizeof(remoteSockAddress);

        ucx(::ucp_ep_create, "create endpoint", _raw, &params, &endpoint);
        _endpoint.emplace(endpoint);

        beginHandshake();
    }

    void UCPWorker::disconnect()
    {
        if (!_endpoint)
        {
            return;
        }

        if (_disconnectReq)
        {
            return;
        }

        _disconnectReq = ucxReq(
            ::ucp_ep_close_nb, "disconnect", *_endpoint, UCP_EP_CLOSE_MODE_FLUSH);
    }

    void UCPWorker::transferGrain(uint64_t index)
    {
        if (_sendRequest || _putRequest)
        {
            throw std::runtime_error("not ready to send another grain");
        }

        // Noop if already unpacked
        // This needs to happen here, after writeup is completed, because apparently the rkeys get
        // invalidated if they get unpacked before the first message is exchanged
        unpackRKeys();

        // Post the ucx_put_nbx() operation.
        postPutOp(index);

        // If we can use ::ucp_worker_fence then we do it here, and post the grain index send
        // right away. Otherwise, it will be posted after the put has completed.
        if (_useFence)
        {
            // fence to make sure, the write completes before the send
            ucx(::ucp_worker_fence, "ucp_worker_fence", _raw);

            postGrainIndexSend();
        }
    }

    std::optional<uint64_t> UCPWorker::receiveGrainBlocking(std::chrono::milliseconds timeout)
    {
        if (!_endpoint)
        {
            throw std::runtime_error("not connected");
        }

        // If the request has not been posted yet.
        if (!_recvRequest)
        {
            postGrainIndexRecv();

            // If there is no request, it has completed right away
            if (!_recvRequest)
            {
                return _inFlightGrainIndex;
            }
        }

        // Block until the timeout.
        makeProgressBlocking(timeout);

        // If the request still exists, return nullopt
        if (_recvRequest)
        {
            return std::nullopt;
        }

        return _inFlightGrainIndex;
    }

    std::optional<uint64_t> UCPWorker::receiveGrainNonBlocking()
    {
        if (!_endpoint)
        {
            throw std::runtime_error("not connected");
        }

        postGrainIndexRecv();
        makeProgress();

        if (_recvRequest)
        {
            return std::nullopt;
        }

        return _inFlightGrainIndex;
    }

    bool UCPWorker::makeProgress()
    {
        if (::ucp_worker_progress(_raw) > 0)
        {
            afterProgress();
        }

        return hasWork();
    }

    bool UCPWorker::makeProgressBlocking(std::chrono::milliseconds timeout)
    {
        if (!hasWork())
        {
            return false;
        }

        bool someProgress = false;
        while (::ucp_worker_arm(_raw) == UCS_ERR_BUSY)
        {
            while (::ucp_worker_progress(_raw))
            {
                someProgress = true;
            }
        }

        if (someProgress)
        {
            afterProgress();
            return hasWork();
        }

        ::pollfd pfd = {
            .fd = _efd,
            .events = POLLIN | POLLERR,
            .revents = 0,
        };

        auto npoll = ::poll(&pfd, 1, timeout.count());
        if (npoll == -1)
        {
            if (errno == EINTR)
            {
                return hasWork();
            }

            throw std::runtime_error(
                fmt::format("poll ucp worker event fd: {}", ::strerror(errno)));
        }

        if (npoll > 0)
        {
            while (::ucp_worker_progress(_raw) > 0)
            {
            }

            afterProgress();
        }

        return hasWork();
    }

    std::string UCPWorker::getTargetInfo()
    {
        if (!_bindAddress)
        {
            throw std::runtime_error("not listening");
        }

        picojson::object obj{};
        obj.emplace("sockaddr", *_bindAddress);
        auto [regions, _] = obj.emplace("regions", picojson::array{});

        for (auto const& reg : _localRegions)
        {
            picojson::object obj{};
            obj.emplace("address", static_cast<double>(reinterpret_cast<std::uintptr_t>(reg.addr)));
            obj.emplace("size", static_cast<double>(reg.size));
            obj.emplace("rkey", reg.rkey);

            regions->second.get<picojson::array>().emplace_back(std::move(obj));
        }

        return picojson::value{std::move(obj)}.serialize(false);
    }

    void UCPWorker::importTargetInfo(std::string const& targetInfo)
    {
        MXL_INFO("{}: import: {}", _name, targetInfo);

        picojson::value v{};
        auto error = picojson::parse(v, targetInfo);
        if (!error.empty())
        {
            throw std::runtime_error(fmt::format("parse target info: {}", error));
        }

        auto const& root = v.get<picojson::object>();

        // import the remote socket address from the json object
        _remoteAddress = root.at("sockaddr").get<std::string>();

        // import individual memory regions from json object and push them to
        // local regions
        for (auto const& regionv : root.at("regions").get<picojson::array>())
        {
            auto const& region = regionv.get<picojson::object>();
            auto const& address = region.at("address").get<double>();
            auto const& size = region.at("size").get<double>();
            auto const& rkey = region.at("rkey").get<std::string>();

            MXL_INFO("{}: adding remote region 0x{:x} - 0x{:x}",
                _name,
                static_cast<std::uintptr_t>(address),
                static_cast<std::uintptr_t>(address) + static_cast<std::uintptr_t>(size));

            _remoteRegions.emplace_back(static_cast<std::uintptr_t>(address),
                static_cast<std::size_t>(size),
                rkey,
                nullptr);
        }
    }

    void UCPWorker::unpackRKeys()
    {
        if (_rkeysReady)
        {
            return;
        }

        MXL_INFO("unpacking rkeys");

        for (auto& region : _remoteRegions)
        {
            auto rkeyData = base64::from_base64(region.rkeyp);
            ucx(::ucp_ep_rkey_unpack, "unpack rkey", *_endpoint, rkeyData.data(), &region.rkey);
        }

        _rkeysReady = true;
    }

    void UCPWorker::destroyEndpoint()
    {
        for (auto& region : _remoteRegions)
        {
            if (region.rkey != nullptr)
            {
                ::ucp_rkey_destroy(region.rkey);
            }

            region.rkey = nullptr;
        }

        _endpoint.reset();
    }

    bool UCPWorker::isConnected() const noexcept
    {
        return _endpoint.has_value() && (_connectionWaitFlags == 0);
    }

    void UCPWorker::afterProgress() noexcept
    {
        if (_disconnectReq && ::ucp_request_check_status(_disconnectReq) != UCS_INPROGRESS)
        {
            ::ucp_request_free(_disconnectReq);

            MXL_INFO("endpoint disconnected");
            _disconnectReq = nullptr;
            destroyEndpoint();
        }

        if (_sendRequest && ::ucp_request_check_status(_sendRequest) != UCS_INPROGRESS)
        {
            ::ucp_request_free(_sendRequest);
            _sendRequest = nullptr;

            if (_connectionWaitFlags & ConnectionFlags::WAIT_CONNECTED_SEND)
            {
                _connectionWaitFlags &= ~(ConnectionFlags::WAIT_CONNECTED_SEND);
            }
        }

        if (_putRequest && ::ucp_request_check_status(_putRequest) != UCS_INPROGRESS)
        {
            ::ucp_request_free(_putRequest);
            _putRequest = nullptr;

            if (!_useFence)
            {
                postGrainIndexSend();
            }
        }

        if (_recvRequest &&
            ::ucp_stream_recv_request_test(_recvRequest, &_recvLen) != UCS_INPROGRESS)
        {
            ::ucp_request_free(_recvRequest);
            _recvRequest = nullptr;

            if (_connectionWaitFlags & ConnectionFlags::WAIT_CONNECTED_RECV)
            {
                _connectionWaitFlags &= ~(ConnectionFlags::WAIT_CONNECTED_RECV);
                _remoteNameBuffer.resize(_recvLen);
                MXL_INFO("{}: connection established to {}", _name, _remoteNameBuffer);
            }
            else
            {
                auto& region = _localRegions[_inFlightGrainIndex % _localRegions.size()];

                // place the index at the end of the buffer
                auto indexInBuffer = *reinterpret_cast<uint64_t*>(
                    reinterpret_cast<std::uintptr_t>(region.addr) +
                    (region.size - sizeof(_inFlightGrainIndex)));

                assert(indexInBuffer == _inFlightGrainIndex);
            }
        }
    }

    bool UCPWorker::hasWork() const noexcept
    {
        return _recvRequest || _sendRequest || _putRequest || _disconnectReq ||
               _connectionWaitFlags || _listener.has_value();
    }

    void UCPWorker::postPutOp(uint64_t index)
    {
        _inFlightGrainIndex = index;

        auto const& localRegion = _localRegions[index % _localRegions.size()];
        auto const& remoteRegion = _remoteRegions[index % _remoteRegions.size()];

        // place the index at the end of the buffer
        *reinterpret_cast<uint64_t*>(reinterpret_cast<std::uintptr_t>(localRegion.addr) +
                                     (localRegion.size - sizeof(index))) = index;

        ::ucp_request_param_t putParams;
        putParams.op_attr_mask = 0;
        _putRequest = ucxReq(::ucp_put_nbx,
            "submit grain write op",
            *_endpoint,
            localRegion.addr,
            std::max(localRegion.size, remoteRegion.size),
            remoteRegion.addr,
            remoteRegion.rkey,
            &putParams);
    }

    void UCPWorker::postGrainIndexSend()
    {
        ::ucp_request_param_t streamSendParams;
        streamSendParams.op_attr_mask = 0;
        _sendRequest = ucxReq(::ucp_stream_send_nbx,
            "ucp stream send",
            *_endpoint,
            reinterpret_cast<void*>(&_inFlightGrainIndex),
            sizeof(_inFlightGrainIndex),
            &streamSendParams);
    }

    void UCPWorker::postGrainIndexRecv()
    {
        ::ucp_request_param_t params;
        params.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
        params.flags = UCP_STREAM_RECV_FLAG_WAITALL;

        _recvRequest = ucxReq(ucp_stream_recv_nbx,
            "ucp stream send",
            *_endpoint,
            reinterpret_cast<void*>(&_inFlightGrainIndex),
            sizeof(_inFlightGrainIndex),
            &_recvLen,
            &params);
    }

    void UCPWorker::beginHandshake()
    {
        ::ucp_request_param_t recvParams{};
        ::ucp_request_param_t sendParams{};

        // post a receive for the handshake message from remote
        _remoteNameBuffer.resize(1024, '\0');
        _recvRequest = ucxReq(::ucp_stream_recv_nbx,
            "receive initial data",
            *_endpoint,
            _remoteNameBuffer.data(),
            _remoteNameBuffer.size(),
            &_recvLen,
            &recvParams);

        _sendRequest = ucxReq(::ucp_stream_send_nbx,
            "send initial data",
            *_endpoint,
            _name.data(),
            _name.size(),
            &sendParams);

        if (_recvRequest != nullptr)
        {
            _connectionWaitFlags |= ConnectionFlags::WAIT_CONNECTED_SEND;
        }

        if (_sendRequest != nullptr)
        {
            _connectionWaitFlags |= ConnectionFlags::WAIT_CONNECTED_RECV;
        }
    }

    void UCPWorker::handleConnection(::ucp_conn_request_h req)
    {
        ::ucp_ep_params_t params{};
        params.field_mask = UCP_EP_PARAM_FIELD_CONN_REQUEST | UCP_EP_PARAM_FIELD_NAME |
                            UCP_EP_PARAM_FIELD_ERR_HANDLER;
        params.name = _name.c_str();
        params.conn_request = req;
        params.err_handler = ucxHandler<::ucp_err_handler_t>(this, ucpHandleError);

        ::ucp_ep_h endpoint{};
        ucx(::ucp_ep_create, "create endpoint from connection request", _raw, &params, &endpoint);

        _endpoint.emplace(endpoint);

        ::ucp_listener_destroy(*_listener);
        _listener.reset();

        beginHandshake();
    }

    void UCPWorker::handleError(::ucp_ep_h _, ::ucs_status_t err)
    {
        if (err == UCS_ERR_CONNECTION_RESET)
        {
            MXL_INFO("remote endpoint disconnected");
        }

        destroyEndpoint();
    }

}
