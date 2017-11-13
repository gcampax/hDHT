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
#include <iostream>

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
                        const uint8_t* request,
                        size_t request_length,
                        size_t reply_length,
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

std::shared_ptr<Proxy>
Peer::get_proxy(uint64_t object)
{
    auto it = m_proxies.find(object);
    if (it != m_proxies.end()) {
        std::shared_ptr<Proxy> ptr = it->second.lock();
        if (ptr)
            return ptr;
    }

    std::shared_ptr<Proxy> ptr = std::make_shared<Proxy>(shared_from_this(), object);
    m_proxies.insert(std::make_pair(object, ptr));
    return ptr;
}

void
Peer::adopt_connection(Connection* connection)
{
    m_available_connections.push_back(connection);
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

    virtual void new_connection() {
        Connection* connection = new Connection(m_context);
        m_context->new_connection(connection);
    }
};

void
Context::add_address(const net::Address& address)
{
    auto socket = std::make_shared<Server>(this);
    socket->listen(address);
    m_listening_sockets.push_back(socket);
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
        get_peer(address)->adopt_connection(connection);
    } catch(std::exception& e) {
        std::cerr << "Failed to handle new connection: " << e.what() << std::endl;
    }
}

}

}
