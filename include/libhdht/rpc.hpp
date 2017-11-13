/*
  libhdht: a library for Hilbert-curve based Distributed Hash Tables
  Copyright 2017 Keshav Santhanam <santhanam.keshav@gmail.com>
                 Giovanni Campagna <gcampagn@cs.stanford.edu>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 3
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <cassert>

#include "net.hpp"
#include "uv.hpp"

// mini rpc library

namespace libhdht
{

namespace rpc
{

// RPC-level protocol, as opposed to libhdht protocol
namespace protocol
{

struct MessageHeader
{
    uint16_t opcode;
    uint64_t request_id;
} __attribute__((packed));

struct BaseRequest
{
    MessageHeader header;
    uint64_t object_id;
} __attribute__((packed));

struct BaseResponse {
    MessageHeader header;
    uint32_t error;
};

}

class Error : public std::runtime_error
{
protected:
    Error(const char* msg) : std::runtime_error(msg) {}
};

class NetworkError : public Error
{
private:
    uv::Error m_error;

public:
    NetworkError(const uv::Error& error) : Error(error.what()), m_error(error) {}
};

class RemoteError : public Error
{
private:
    int code;

public:
    RemoteError(int code) : Error(strerror(code)) {}
};

class Connection;
class Peer;
class Proxy;
class Stub;
class Server;
class Context;

class Peer : public std::enable_shared_from_this<Peer>
{
    friend class Context;
    friend class Connection;

private:
    static const unsigned MAX_CONNECTIONS = 3;

    struct OutstandingRequest {
        uv::Buffer request_buffer;
        std::function<void(Error*, const uv::Buffer*)> callback;
    };

    const Context *m_context;
    const net::Address m_address;
    std::vector<Connection*> m_available_connections;
    std::unordered_map<uint64_t, std::weak_ptr<Proxy>> m_proxies;
    std::unordered_map<uint64_t, std::shared_ptr<Stub>> m_stubs;
    std::unordered_map<uint64_t, OutstandingRequest> m_requests;
    uint64_t m_next_stub_id = 0;
    uint64_t m_next_req_id = 0;
    unsigned m_next_connection = 0;

    Connection* get_connection();
    void destroy_stub(uint64_t stub_id)
    {
        m_stubs.erase(stub_id);
    }

    void adopt_connection(Connection*);
    void drop_connection(Connection*);

public:
    Peer(Context *ctx, const net::Address& address) : m_context(ctx), m_address(address) {}

    const net::Address& get_address() const
    {
        return m_address;
    }

    std::shared_ptr<Stub> get_stub(uint64_t object)
    {
        auto it = m_stubs.find(object);
        if (it != m_stubs.end())
            return it->second;
        else
            return nullptr;
    }

    template<typename T>
    std::shared_ptr<T> get_proxy(uint64_t object)
    {
        auto it = m_proxies.find(object);
        if (it != m_proxies.end()) {
            std::shared_ptr<Proxy> ptr = it->second.lock();
            if (ptr)
                return std::dynamic_pointer_cast<T>(ptr);
        }

        std::shared_ptr<T> ptr = std::make_shared<T>(shared_from_this(), object);
        m_proxies.insert(std::make_pair(object, ptr));
        return ptr;
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> create_stub(Args&&... args)
    {
        uint64_t stub_id = m_next_stub_id ++;
        std::shared_ptr<T> stub = std::make_shared<T>(shared_from_this(), stub_id, std::forward<Args>(args)...);
        m_stubs.insert(std::make_pair(stub_id, stub));
        return stub;
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> create_named_stub(uint64_t object_id, Args&&... args)
    {
        m_next_stub_id = std::max(m_next_stub_id, object_id+1);
        std::shared_ptr<T> stub = std::make_shared<T>(shared_from_this(), object_id, std::forward<Args>(args)...);
        auto result = m_stubs.insert(std::make_pair(object_id, stub));
        assert(result.second);
        return stub;
    }

    void invoke_request(uint16_t opcode,
                        uint64_t object_id,
                        uv::Buffer&& request,
                        const std::function<void(Error*, const uv::Buffer*)>&);
    void send_error(uint16_t opcode,
                    uint64_t request_id,
                    rpc::RemoteError error);
    void send_reply(uint16_t opcode,
                    uint64_t request_id,
                    uv::Buffer&& reply);

    /*template<typename Request, typename Callback>
    void invoke_request(uint64_t object_id,
                        const typename Request::request_type& req,
                        Callback&& callback)
    {
        struct CallbackWrapper {
            Callback m_callback;
            CallbackWrapper(Callback&& callback) : m_callback(std::forward<Callback>(callback)) {}

            void operator()(Error* error, const uv::Buffer* reply) const {
                if (error != nullptr)
                    m_callback(error, nullptr);
                else
                    m_callback(nullptr, Request::reply_type::unmarshal(*reply));
            }
        } callback_wrapper(std::forward<Callback>(callback));

        uv::Buffer buf(req.marshal());
        invoke_request(Request::opcode, object_id, std::move(buf),
            sizeof(typename Request::reply_type), callback_wrapper);
    }*/
};

class Proxy : public std::enable_shared_from_this<Proxy>
{
private:
    std::shared_ptr<Peer> m_peer;
    uint64_t m_object_id;

protected:
    std::shared_ptr<Peer> get_peer()
    {
        return m_peer;
    }

public:
    Proxy(std::shared_ptr<Peer> peer, uint64_t object_id) : m_peer(peer), m_object_id(object_id) {}

    uint64_t get_object_id() const
    {
        return m_object_id;
    }

    // only need this for RTTI (which is used to check the types
    // of arguments on the wire)
    virtual ~Proxy() {}
};

class Stub : public std::enable_shared_from_this<Stub>
{
private:
    std::weak_ptr<Peer> m_peer;
    uint64_t m_object_id;

protected:
    std::shared_ptr<Peer> get_peer()
    {
        return m_peer.lock();
    }
    void reply_error(uint16_t opcode, uint64_t request_id, rpc::RemoteError error)
    {
        std::shared_ptr<Peer> peer = get_peer();
        if (!peer) {
            log(LOG_ERR, "Error reply dropped (peer was garbage collected)");
            return;
        }
        peer->send_error(opcode, request_id, error);
    }

public:
    Stub(std::shared_ptr<Peer> peer, uint64_t object_id) : m_peer(peer), m_object_id(object_id) {}
    virtual ~Stub();

    uint64_t get_object_id() const
    {
        return m_object_id;
    }

    virtual void dispatch_request(int16_t opcode, uint64_t request_id, const uv::Buffer& buffer) = 0;
};

class Context
{
    friend class Server;

private:
    uv::Loop& m_loop;
    std::vector<std::unique_ptr<uv::TCPSocket>> m_listening_sockets;
    std::unordered_map<net::Address, std::weak_ptr<Peer>> m_known_peers;
    std::vector<std::function<void(std::shared_ptr<Peer>)>> m_stub_factories;

    void new_connection(Connection*);

public:
    Context(uv::Loop& loop) : m_loop(loop) {}

    template<typename Callback>
    void add_stub_factory(Callback&& callback)
    {
        m_stub_factories.emplace_back(std::forward<Callback>(callback));
    }
    void add_address(const net::Address& address);
    std::shared_ptr<Peer> get_peer(const net::Address& address);

    uv::Loop& get_event_loop()
    {
        return m_loop;
    }
};

}

}
