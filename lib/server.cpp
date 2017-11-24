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

#include <cassert>

namespace libhdht
{

class ServerMasterImpl : public protocol::ServerStub {
public:
    ServerMasterImpl(std::shared_ptr<rpc::Peer> peer, uint64_t object_id) : protocol::ServerStub(peer, object_id)
    {
        assert(object_id == protocol::MASTER_OBJECT_ID);
    }

    virtual void handle_server_hello(uint64_t request_id, net::Address server_address) override
    {
        auto peer = get_peer();
        log(LOG_INFO, "Received ServerHello from %s", server_address.to_string().c_str());
        peer->add_listening_address(server_address);

        // TODO: do something with it
        reply_server_hello(request_id);
    }

    virtual void handle_client_hello(uint64_t request_id, net::Address client_address) override
    {
        auto peer = get_peer();
        log(LOG_INFO, "Received ClientHello from %s", client_address.to_string().c_str());
        peer->add_listening_address(client_address);

        // TODO: do something with it
        reply_client_hello(request_id);
    }

    virtual void handle_control_range(uint64_t request_id, NodeIDRange range) override
    {

    }

    virtual void handle_find_controlling_server(uint64_t request_id, NodeID id) override
    {

    }

    virtual void handle_set_location(uint64_t request_id, double latitude, double longitude) override
    {

    }

    virtual void handle_find_client_address(uint64_t request_id, NodeID node_id) override
    {

    }
};

ServerContext::ServerContext(uv::Loop& loop) : m_rpc(loop), m_table(0)
{
    m_rpc.add_stub_factory([](std::shared_ptr<rpc::Peer> peer) {
        peer->create_named_stub<ServerMasterImpl>(protocol::MASTER_OBJECT_ID);
    });
}

void
ServerContext::add_address(const net::Address& address)
{
    m_rpc.add_address(address);
}

void
ServerContext::add_peer(const net::Address& address)
{
    m_peers.push_back(address);
}

void
ServerContext::start()
{
    net::Address own_address = m_rpc.get_listening_address();

    // register ourselves with the peers we know about
    for (auto address : m_peers) {
        auto peer = m_rpc.get_peer(address);

        auto self = std::static_pointer_cast<ServerMasterImpl>(peer->get_stub(protocol::MASTER_OBJECT_ID));
        auto master = peer->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);
        master->invoke_server_hello([peer, address](rpc::Error *error) {
            if (error) {
                log(LOG_WARNING, "Failed to register with %s: %s", address.to_string().c_str(), error->what());
            } else {
                log(LOG_INFO, "Registered with %s successfully", address.to_string().c_str());
            }
        }, own_address);
    }
}

}
