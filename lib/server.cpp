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

#include "libhdht-private.hpp"

#include <cassert>

namespace libhdht
{

static std::shared_ptr<protocol::ServerProxy> maybe_register_with_server(rpc::Context *ctx, const net::Address& address);

class ServerMasterImpl : public protocol::ServerStub {
private:
    rpc::Context *m_rpc;
    Table *m_table;
    bool is_server = false;
    bool is_client = false;
    ClientNode *m_client_node = nullptr;

    // register the peer corresponding to this stub as client or server
    void register_server()
    {
        if (is_client)
            throw rpc::RemoteError(EPERM);
        is_server = true;
    }
    void register_client()
    {
        if (is_server)
            throw rpc::RemoteError(EPERM);
        is_client = true;
    }

    // check that the peer corresponding to this stub registered as client or
    // server
    void check_client_or_server()
    {
        if (!is_server && !is_client)
            throw rpc::RemoteError(EPERM);
    }
    void check_server()
    {
        if (!is_server)
            throw rpc::RemoteError(EPERM);
    }
    void check_client()
    {
        if (!is_client)
            throw rpc::RemoteError(EPERM);
    }

public:
    ServerMasterImpl(std::shared_ptr<rpc::Peer> peer, uint64_t object_id, rpc::Context *rpc, Table *table) :
        protocol::ServerStub(peer, object_id),
        m_rpc(rpc),
        m_table(table)
    {
        assert(object_id == protocol::MASTER_OBJECT_ID);
    }

    virtual void handle_server_hello(uint64_t request_id, net::Address server_address) override
    {
        auto peer = get_peer();
        log(LOG_INFO, "Received ServerHello from %s", server_address.to_string().c_str());
        peer->add_listening_address(server_address);
        register_server();

        // TODO: do something with it
        reply_server_hello(request_id);
    }

    virtual void handle_client_hello(uint64_t request_id, net::Address client_address, GeoPoint2D point) override
    {
        auto peer = get_peer();
        log(LOG_INFO, "Received ClientHello from %s", client_address.to_string().c_str());
        peer->add_listening_address(client_address);
        register_client();
        m_client_node = m_table->get_or_create_client_node(point);
        if (m_client_node)
            m_client_node->set_peer(peer);

        // m_client_node can be nullptr here, in case the point falls outside the ranges controlled
        // by this server
        reply_client_hello(request_id, m_client_node != nullptr, m_client_node ? m_client_node->get_id() : NodeID());
    }

    virtual void handle_add_remote_range(uint64_t request_id, NodeIDRange range, net::Address address) override
    {
        check_server();
        if (!m_table->is_valid_range(range))
            throw rpc::RemoteError(EINVAL);

        auto proxy = maybe_register_with_server(m_rpc, address);

        m_table->add_remote_server_node(range, proxy);
    }

    virtual void handle_control_range(uint64_t request_id, NodeIDRange range) override
    {
        check_server();
        if (!m_table->is_valid_range(range))
            throw rpc::RemoteError(EINVAL);

        m_table->add_local_server_node(range);
    }

    virtual void handle_adopt_client(uint64_t request_id, NodeID node_id, net::Address address, std::unordered_map<std::string, std::string> metadata) override
    {
        check_server();
        if (!m_table->is_valid_node_id(node_id))
            throw rpc::RemoteError(EINVAL);

        ClientNode *client_node = m_table->get_or_create_client_node(node_id);
        if (client_node == nullptr)
            throw rpc::RemoteError(EACCES);

        auto peer = m_rpc->get_peer(address);
        client_node->set_peer(peer);
        client_node->set_all_metadata(std::move(metadata));
    }

    virtual void handle_find_controlling_server(uint64_t request_id, NodeID id) override
    {
        check_client_or_server();
        if (!m_table->is_valid_node_id(id))
            throw rpc::RemoteError(EINVAL);

        ServerNode *node = m_table->find_controlling_server(id);

        if (node->is_local()) {
            reply_find_controlling_server(request_id, m_rpc->get_listening_address(), node->get_range());
            return;
        }

        auto proxy = dynamic_cast<RemoteServerNode*>(node)->get_proxy();
        auto self = shared_from_this();
        proxy->invoke_find_controlling_server([self, request_id, id, this](rpc::Error *err, const net::Address& address, const NodeIDRange& subrange) {
            if (err) {
                auto remote_err = dynamic_cast<rpc::RemoteError*>(err);
                if (remote_err)
                    reply_error(request_id, *remote_err);
                else
                    reply_error(request_id, EHOSTUNREACH); // Generic network failure
                return;
            }

            ServerNode *node = m_table->find_controlling_server(id);
            if (node->is_local()) {
                // race condition: we became leader for this range while were asking someone about iter
                reply_find_controlling_server(request_id, m_rpc->get_listening_address(), node->get_range());
                return;
            }

            if (!node->get_range().contains(subrange)) {
                // the other peer is being weird
                reply_error(request_id, EIO);
                return;
            }

            if (address == m_rpc->get_listening_address()) {
                // the other peer has a very confused view of the world
                reply_error(request_id, ELOOP);
                return;
            }

            auto subproxy = maybe_register_with_server(m_rpc, address);
            m_table->add_remote_server_node(subrange, subproxy);
            reply_find_controlling_server(request_id, address, subrange);
        }, id);
    }

    virtual void handle_set_location(uint64_t request_id, GeoPoint2D new_location) override
    {
        check_client();

        if (m_client_node == nullptr)
            throw rpc::RemoteError(ENXIO);

        ServerNode *new_server = m_table->move_client(m_client_node, new_location);
        if (new_server->is_local()) {
            reply_set_location(request_id, protocol::SetLocationResult::SameServer);
            return;
        }

        auto proxy = static_cast<RemoteServerNode*>(new_server)->get_proxy();
        auto self = shared_from_this();
        proxy->invoke_adopt_client([self, request_id, this](rpc::Error *err) {
            ClientNode *client_node = m_client_node;
            m_client_node = nullptr;

            if (err) {
                auto client_name = m_client_node->get_address().to_string();
                log(LOG_ERR, "Failed to transfer client %s: %s", client_name.c_str(), err->what());

                auto remote_err = dynamic_cast<rpc::RemoteError*>(err);
                if (remote_err)
                    reply_error(request_id, *remote_err);
                else
                    reply_error(request_id, EHOSTUNREACH); // Generic network failure
                return;
            }

            m_table->forget_client(client_node);
            reply_set_location(request_id, protocol::SetLocationResult::DifferentServer);
        }, m_client_node->get_id(), m_client_node->get_address(), m_client_node->get_all_metadata());
    }

    virtual void handle_set_metadata(uint64_t request_id, std::string key, std::string value) override
    {
        check_client();

        if (m_client_node == nullptr)
            throw rpc::RemoteError(ENXIO);

        m_client_node->set_metadata(key, value);
        reply_set_metadata(request_id);
    }

    virtual void handle_find_client_address(uint64_t request_id, NodeID node_id) override
    {
        check_client();
        if (!m_table->is_valid_node_id(node_id))
            throw rpc::RemoteError(EINVAL);

        ClientNode *node = m_table->get_or_create_client_node(node_id);
        if (node == nullptr) // this node ID is not here, call find_controlling_server() first
            throw rpc::RemoteError(ENOENT);

        assert(node->get_peer());
        reply_find_client_address(request_id, node->get_address());
    }
};

ServerContext::ServerContext(uv::Loop& loop) :
    m_rpc(std::make_unique<rpc::Context>(loop)),
    m_table(std::make_unique<Table>(0))
{
    m_rpc->add_stub_factory([this](std::shared_ptr<rpc::Peer> peer) {
        peer->create_named_stub<ServerMasterImpl>(protocol::MASTER_OBJECT_ID, m_rpc.get(), m_table.get());
    });
}

ServerContext::~ServerContext()
{}

void
ServerContext::add_address(const net::Address& address)
{
    m_rpc->add_address(address);
}

void
ServerContext::add_peer(const net::Address& address)
{
    m_peers.push_back(address);
}

std::shared_ptr<protocol::ServerProxy>
maybe_register_with_server(rpc::Context *ctx, const net::Address& address)
{
    if (ctx->has_peer(address)) {
        auto peer = ctx->get_peer(address);
        return peer->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);
    }

    net::Address own_address = ctx->get_listening_address();
    auto peer = ctx->get_peer(address);
    auto master = peer->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);
    master->invoke_server_hello([peer, address](rpc::Error *error) {
        if (error) {
            log(LOG_WARNING, "Failed to register with %s: %s", address.to_string().c_str(), error->what());
        } else {
            log(LOG_INFO, "Registered with %s successfully", address.to_string().c_str());
        }
    }, own_address);
    return master;
}

void
ServerContext::start()
{
    // register ourselves with the peers we know about
    for (auto address : m_peers)
        maybe_register_with_server(m_rpc.get(), address);
}

}
