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

#include <libhdht/libhdht.hpp>
#include <libhdht/rpc.hpp>

#include <new>

namespace libhdht
{

namespace rpc
{

class Connection : public uv::TCPSocket
{
private:
    Context* m_context;
    std::shared_ptr<Peer> m_peer;

public:
    Connection(Context *ctx) : uv::TCPSocket(ctx->get_event_loop()) {
        m_context = ctx;
    }
    Connection(Context *ctx, const net::Address& address) : uv::TCPSocket(ctx->get_event_loop()) {
        m_context = ctx;
        connect(address);
    }

    void set_peer(std::shared_ptr<Peer> peer)
    {
        m_peer = peer;
    }
    std::shared_ptr<Peer> get_peer()
    {
        return m_peer;
    }

    void invoke_request(int16_t opcode,
                        uint64_t object_id,
                        const uv::Buffer& buffer,
                        void (*)(int error, const uv::Buffer& reply));

    virtual void closed() override
    {
        if (m_peer)
            m_peer->drop_connection(this);
        uv::TCPSocket::closed();
    }
    virtual void read_callback(uv::Error err, const uv::Buffer&) override
    {
        if (err) {
            std::string name(get_peer_name().to_string());
            if (err == UV_EOF) {
                log(LOG_NOTICE, "Connection with peer %s closed", name.c_str());
            } else {
                log(LOG_WARNING, "Read error from peer %s: %s", name.c_str(), err.what());
            }
            close();
            return;
        }

        // do something
    }
    virtual void write_complete(uint64_t req_id, uv::Error err) override
    {
        if (err) {
            std::string name(get_peer_name().to_string());
            log(LOG_WARNING, "Write error to peer %s: %s", name.c_str(), err.what());
            close();
        }

        // do something
    }
};

void
Peer::adopt_connection(Connection* connection)
{
    m_available_connections.push_back(connection);
    connection->start_reading();
}

void
Peer::drop_connection(Connection* connection)
{
    for (auto it = m_available_connections.begin(); it != m_available_connections.end(); it++) {
        if (*it == connection) {
            m_available_connections.erase(it);
            return;
        }
    }
}

void
Peer::invoke_request(uint16_t opcode,
                     uint64_t object_id,
                     uv::Buffer&& request,
                     const std::function<void(Error*, const uv::Buffer*)>&)
{
    // TODO do something with this
}

void
Peer::send_error(uint16_t opcode,
                 uint64_t request_id,
                 rpc::RemoteError error)
{
    // TODO do something with this
}

void
Peer::send_reply(uint16_t opcode,
                 uint64_t request_id,
                 uv::Buffer&& reply)
{
}

class Server : public uv::TCPSocket
{
private:
    Context *m_context;

public:
    Server(Context *ctx) : uv::TCPSocket(ctx->get_event_loop()), m_context(ctx) {}
    Server(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(const Server&) = delete;
    Server& operator=(Server&&) = delete;

    virtual void new_connection() override {
        Connection* connection = new Connection(m_context);
        accept(connection);
        m_context->new_connection(connection);
    }
};

void
Context::add_address(const net::Address& address)
{
    auto socket = std::make_unique<Server>(this);
    socket->listen(address);
    socket->start_reading();
    m_listening_sockets.push_back(std::move(socket));

    log(LOG_INFO, "Listening on address %s", address.to_string().c_str());
}

std::shared_ptr<Peer>
Context::get_peer(const net::Address& address)
{
    auto it = m_known_peers.find(address);
    if (it != m_known_peers.end()) {
        std::shared_ptr<Peer> ptr = it->second.lock();
        if (ptr)
            return ptr;
    }

    std::shared_ptr<Peer> ptr = std::make_shared<Peer>(this, address);
    for (auto& factory : m_stub_factories)
        factory(ptr);
    m_known_peers.insert(std::make_pair(address, ptr));

    return ptr;
}

void
Context::new_connection(Connection* connection)
{
    try {
        net::Address address = connection->get_peer_name();
        log(LOG_INFO, "New connection from %s", address.to_string().c_str());

        get_peer(address)->adopt_connection(connection);
    } catch(std::exception& e) {
        log(LOG_WARNING, "Failed to handle new connection: %s", e.what());
    }
}

// empty destructor, just to fill in the vtable slot
Stub::~Stub()
{}

}

}
