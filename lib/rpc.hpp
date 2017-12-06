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
#include <limits>
#include <unordered_map>

#include <libhdht/net.hpp>
#include <libhdht/uv.hpp>

// mini rpc library

namespace libhdht
{

namespace rpc
{

// RPC-level protocol, as opposed to libhdht protocol
namespace protocol
{

static const uint16_t REPLY_FLAG = 1<<15;
static const size_t MAX_PAYLOAD_SIZE = std::numeric_limits<uint16_t>::max();

struct MessageHeader
{
    uint16_t opcode;
    uint64_t request_id;
} __attribute__((packed));

struct BaseRequest
{
    MessageHeader header;
    uint64_t object_id;
    uint16_t payload_size;
} __attribute__((packed));

struct BaseResponse
{
    MessageHeader header;
    uint32_t error;
    uint16_t payload_size;
};

}

namespace impl
{
class Connection;
class Server;
}

class Peer;
class Proxy;
class Stub;
class Context;

class Peer : public std::enable_shared_from_this<Peer>
{
    friend class Context;
    friend class impl::Connection;

private:
    static const unsigned MAX_CONNECTIONS = 3;

    struct OutstandingRequest {
        rpc::RemoteError error;
        uv::Buffer payload;
        std::function<void(Error*, const uv::Buffer*)> callback;
    };
    OutstandingRequest& queue_request(uint64_t request_id, rpc::RemoteError error, uv::Buffer&& payload, const std::function<void(Error*, const uv::Buffer*)>& callback);

    Context *m_context;
    std::unordered_set<net::Address> m_addresses;
    std::vector<impl::Connection*> m_available_connections;
    std::unordered_map<uint64_t, std::weak_ptr<Proxy>> m_proxies;
    std::unordered_map<uint64_t, std::shared_ptr<Stub>> m_stubs;
    std::unordered_map<uint64_t, OutstandingRequest> m_requests;
    uint64_t m_next_stub_id = 0;
    uint64_t m_next_req_id = 0;
    // NOTE(keshav2): Commented out to avoid unused warning
    // unsigned m_next_connection = 0;

    impl::Connection* get_connection();
    void destroy_stub(uint64_t stub_id)
    {
        m_stubs.erase(stub_id);
    }

    void adopt_connection(impl::Connection*);
    void drop_connection(impl::Connection*);

    void write_failed(uint64_t request_id, uv::Error err);
    void reply_received(uint64_t request_id, rpc::RemoteError*, const uv::Buffer* payload);
    void request_received(uint16_t opcode, uint64_t object_id, uint64_t request_id, const uv::Buffer& payload);

public:
    Peer(Context *ctx) : m_context(ctx) {}

    void close_all_connections();

    void add_listening_address(const net::Address& address);
    void remove_listening_address(const net::Address& address);

    net::Address get_listening_address() const
    {
        if (m_addresses.empty())
            return net::Address();
        // FIXME: choose one that is connectable
        return *m_addresses.begin();
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
        (void)result.second;
        return stub;
    }

    void invoke_request(uint16_t opcode,
                        uint64_t object_id,
                        uv::Buffer&& request,
                        const std::function<void(Error*, const uv::Buffer*)>&);
    void send_error(uint64_t request_id,
                    rpc::RemoteError error);
    void send_fatal_error(uint64_t request_id,
                          rpc::RemoteError error);
    void send_reply(uint64_t request_id,
                    uv::Buffer&& reply);
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
    net::Address get_address() const
    {
        return m_peer->get_listening_address();
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
    void reply_error(uint64_t request_id, rpc::RemoteError error)
    {
        std::shared_ptr<Peer> peer = get_peer();
        if (!peer) {
            log(LOG_ERR, "Error reply dropped (peer was garbage collected)");
            return;
        }
        peer->send_error(request_id, error);
    }
    void reply_fatal_error(uint64_t request_id, rpc::RemoteError error)
    {
        std::shared_ptr<Peer> peer = get_peer();
        if (!peer) {
            log(LOG_ERR, "Error reply dropped (peer was garbage collected)");
            return;
        }
        peer->send_fatal_error(request_id, error);
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
    friend class impl::Server;

private:
    uv::Loop& m_loop;
    std::vector<std::unique_ptr<impl::Server>> m_listening_sockets;
    std::unordered_map<net::Address, std::weak_ptr<Peer>> m_known_peers;
    std::vector<std::function<void(std::shared_ptr<Peer>)>> m_stub_factories;

    void new_connection(impl::Connection*);

public:
    enum class AddressType { Static, Dynamic };

    Context(uv::Loop& loop);
    ~Context();

    template<typename Callback>
    void add_stub_factory(Callback&& callback)
    {
        m_stub_factories.emplace_back(std::forward<Callback>(callback));
    }
    void add_address(const net::Address& address);
    net::Address get_listening_address() const;

    bool has_peer(const net::Address& address) const
    {
        return m_known_peers.find(address) != m_known_peers.end();
    }
    std::shared_ptr<Peer> get_peer(const net::Address& address, AddressType type = AddressType::Static);

    void remove_peer_address(std::shared_ptr<Peer> peer, const net::Address& address);
    void add_peer_address(std::shared_ptr<Peer> peer, const net::Address& address);

    uv::Loop& get_event_loop() const
    {
        return m_loop;
    }
};

}

}
