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

class MasterImpl : public protocol::MasterStub
{
public:
    MasterImpl(std::shared_ptr<rpc::Peer> peer, uint64_t object_id) : protocol::MasterStub(peer, object_id)
    {
        assert(object_id == protocol::MASTER_OBJECT_ID);
    }

    virtual void handle_server_hello(uint64_t request_id, std::shared_ptr<protocol::MasterProxy> other) override
    {
        if (other == nullptr)
            throw rpc::RemoteError(EINVAL);

        auto peer = get_peer();
        log(LOG_INFO, "Received ServerHello from %s", peer->get_address().to_string().c_str());

        // TODO: do something with it
        reply_server_hello(request_id);
    }

    virtual void handle_register_server_node(uint64_t request_id, NodeID node_id, std::shared_ptr<protocol::ServerNodeProxy> node) override
    {
        log(LOG_ERR, "register_server_node: not implemented yet");
        throw rpc::RemoteError(ENOSYS);
    }

    virtual void handle_register_client_node(uint64_t request_id, double latitude, double longitude, std::shared_ptr<protocol::ClientNodeProxy> node) override
    {
        log(LOG_ERR, "register_client_node: not implemented yet");
        throw rpc::RemoteError(ENOSYS);
    }
};

ServerContext::ServerContext(uv::Loop& loop) : m_rpc(loop)
{
    m_rpc.add_stub_factory([](std::shared_ptr<rpc::Peer> peer) {
        peer->create_named_stub<MasterImpl>(protocol::MASTER_OBJECT_ID);
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
    // register ourselves with the peers we know about
}

}
