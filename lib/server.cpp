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

    void send_node_to_peer(ServerNode *node, std::shared_ptr<protocol::ServerProxy> proxy)
    {
        net::Address address;

        if (node->is_local()) {
            address = m_rpc->get_listening_address();
        } else {
            auto thirdparty = static_cast<RemoteServerNode*>(node)->get_proxy();
            if (thirdparty == nullptr) {
                // if we don't know the owner, don't send out anything
                log(LOG_DEBUG, "Skipping synchronization for range %s (owner unknown)", node->get_range().to_string().c_str());
                return;
            }
            address = thirdparty->get_address();
        }

        auto range = node->get_range();
        proxy->invoke_add_remote_range([range](rpc::Error *err) {
            if (err) {
                log(LOG_WARNING, "Failed to inform peer of range %s: %s",
                    range.to_string().c_str(), err->what());
            }
        }, range, address);
    }

    void relinquish_node_to_peer(ServerNode *node, std::shared_ptr<protocol::ServerProxy> proxy)
    {
        // we won't tell people to take control of what's not ours
        assert(node->is_local());

        auto range = node->get_range();
        auto self = shared_from_this();
        proxy->invoke_control_range([self, proxy, range, node, this](rpc::Error *err) {
            if (err) {
                log(LOG_WARNING, "Failed to relinquish range %s: %s",
                    range.to_string().c_str(), err->what());
                // take the node back
                m_table->add_local_server_node(range, static_cast<LocalServerNode*>(node));
                return;
            }

            static_cast<LocalServerNode*>(node)->foreach_client([self, proxy, this](ClientNode* client) {
                proxy->invoke_adopt_client([self, client, this](rpc::Error *err) {
                    if (err) {
                        rpc::RemoteError *remote_err = dynamic_cast<rpc::RemoteError*>(err);
                        if (remote_err && remote_err->code() == EACCES) {
                            // FIXME
                            // the table was rebalanced again on their end, and the client needs to go
                            // somewhere else
                        }

                        auto client_name = client->get_address().to_string();
                        log(LOG_ERR, "Failed to transfer client %s: %s", client_name.c_str(), err->what());

                        // not much we can do here, cause we already relinquished the range
                        // just forget about this client
                        // eventually the client will get confused and register again
                        m_table->forget_client(client);
                        return;
                    }

                    // done
                    m_table->forget_client(client);
                }, client->get_id(), client->get_coordinates(), client->get_address(), client->get_all_metadata());
            });

            m_table->forget_server(node);
        }, range);
    }

public:
    ServerMasterImpl(std::shared_ptr<rpc::Peer> peer, uint64_t object_id, rpc::Context *rpc, Table *table) :
        protocol::ServerStub(peer, object_id),
        m_rpc(rpc),
        m_table(table)
    {
        assert(object_id == protocol::MASTER_OBJECT_ID);
    }

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

    virtual void handle_server_hello(uint64_t request_id, net::Address server_address) override
    {
        auto peer = get_peer();
        log(LOG_INFO, "Received ServerHello from %s", server_address.to_string().c_str());
        peer->add_listening_address(server_address);
        register_server();

        auto proxy = peer->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);
        auto self = shared_from_this();
        m_table->load_balance_with_peer(proxy, [self, proxy, this](Table::LoadBalanceAction action, ServerNode *node) {
            switch (action) {
            case Table::LoadBalanceAction::RelinquishRange:
                log(LOG_DEBUG, "Relinquishing range %s to peer %s",
                    node->get_range().to_string().c_str(),
                    proxy->get_address().to_string().c_str());
                relinquish_node_to_peer(node, proxy);
                break;

            case Table::LoadBalanceAction::InformPeer:
                log(LOG_DEBUG, "Informing peer %s of range %s", proxy->get_address().to_string().c_str(),
                    node->get_range().to_string().c_str());
                send_node_to_peer(node, proxy);
                break;
            }
        });

        // TODO: do something with it
        reply_server_hello(request_id);
    }

    virtual void handle_client_hello(uint64_t request_id, net::Address client_address, NodeID existing_node_id, GeoPoint2D point) override
    {
        auto peer = get_peer();
        log(LOG_INFO, "Received ClientHello from %s", client_address.to_string().c_str());
        peer->add_listening_address(client_address);
        register_client();
        point.canonicalize();

        // if this client double registered cause it got confused, just move it to the right place
        protocol::ClientRegistrationResult result;
        if (!m_client_node)
            m_client_node = m_table->get_or_create_client_node(existing_node_id, point);

        if (m_client_node) {
            log(LOG_INFO, "Assuming control of node %s", m_client_node->get_id().to_string().c_str());
            m_client_node->set_peer(peer);

            // if the table obtained an existing ClientNode (same NodeID/geocoordinates,
            // different connection), we already have all the metadata
            if (m_client_node->is_registered()) {
                ServerNode* new_server = m_table->move_client(m_client_node, point);

                // the client should attempt to reregister in the right place
                // we skip the adopt_client() call, so the new server will reply with ClientCreated
                // so the client will upload all the metadata from scratch
                if (!new_server->is_local()) {
                    m_table->forget_client(m_client_node);
                    m_client_node = nullptr;
                    result = protocol::ClientRegistrationResult::WrongServer;
                } else {
                    result = protocol::ClientRegistrationResult::ClientAlreadyExists;
                }
            } else {
                result = protocol::ClientRegistrationResult::ClientCreated;
                m_client_node->set_registered();
            }
        } else {
            log(LOG_INFO, "Rejecting registration, not our responsability");
            result = protocol::ClientRegistrationResult::WrongServer;
        }

        reply_client_hello(request_id, result, m_client_node ? m_client_node->get_id() : NodeID());
    }

    virtual void handle_add_remote_range(uint64_t request_id, NodeIDRange range, net::Address address) override
    {
        check_server();

        log(LOG_INFO, "Found new owner for range %s: %s", range.to_string().c_str(), address.to_string().c_str());
        if (!m_table->is_valid_range(range)) {
            log(LOG_WARNING, "Not a valid range");
            throw rpc::RemoteError(EINVAL);
        }

        auto proxy = maybe_register_with_server(m_rpc, address);

        if (m_table->add_remote_server_node(range, proxy))
            reply_add_remote_range(request_id);
        else
            reply_error(request_id, EACCES);
    }

    virtual void handle_control_range(uint64_t request_id, NodeIDRange range) override
    {
        check_server();

        log(LOG_INFO, "Got request to control range %s", range.to_string().c_str());
        if (!m_table->is_valid_range(range)) {
            log(LOG_WARNING, "Not a valid range");
            throw rpc::RemoteError(EINVAL);
        }

        m_table->add_local_server_node(range);
        reply_control_range(request_id);
    }

    virtual void handle_adopt_client(uint64_t request_id, NodeID node_id, GeoPoint2D point, net::Address address, std::unordered_map<std::string, std::string> metadata) override
    {
        check_server();
        if (!node_id.is_valid())
            throw rpc::RemoteError(EINVAL);

        ClientNode *client_node = m_table->get_or_create_client_node(node_id, point);
        if (client_node == nullptr)
            throw rpc::RemoteError(EACCES);

        auto peer = m_rpc->get_peer(address);
        client_node->set_peer(peer);
        client_node->set_all_metadata(std::move(metadata));
    }

    virtual void handle_find_server_for_point(uint64_t request_id, GeoPoint2D point) override
    {
        check_client();
        handle_find_controlling_server(request_id, m_table->get_node_id_for_point(point));
    }

    virtual void handle_find_controlling_server(uint64_t request_id, NodeID node_id) override
    {
        check_client_or_server();

        log(LOG_INFO, "Received FindControllingServer for %s", node_id.to_string().c_str());

        ServerNode *node = m_table->find_controlling_server(node_id);

        if (node->is_local()) {
            log(LOG_INFO, "Found node locally in range %s", node->get_range().to_string().c_str());
            reply_find_controlling_server(request_id, m_rpc->get_listening_address(), node->get_range());
            return;
        }

        auto proxy = dynamic_cast<RemoteServerNode*>(node)->get_proxy();
        if (proxy == nullptr) {
            log(LOG_WARNING, "Found unknown region in the table: %s", node->get_range().to_string().c_str());
            reply_error(request_id, ENXIO);
            return;
        }
        auto self = shared_from_this();
        proxy->invoke_find_controlling_server([self, request_id, node_id, this](rpc::Error *err, const net::Address& address, const NodeIDRange& subrange) {
            if (err) {
                auto remote_err = dynamic_cast<rpc::RemoteError*>(err);
                if (remote_err)
                    reply_error(request_id, *remote_err);
                else
                    reply_error(request_id, EHOSTUNREACH); // Generic network failure
                return;
            }

            ServerNode *node = m_table->find_controlling_server(node_id);
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
        }, node_id);
    }

    virtual void handle_set_location(uint64_t request_id, GeoPoint2D new_location) override
    {
        check_client();

        if (m_client_node == nullptr)
            throw rpc::RemoteError(ENXIO);

        new_location.canonicalize();
        log(LOG_INFO, "Moving client %s to %s", get_peer()->get_listening_address().to_string().c_str(),
            new_location.to_string().c_str());

        ServerNode *new_server = m_table->move_client(m_client_node, new_location);
        if (new_server->is_local()) {
            log(LOG_INFO, "Client is still under our control");
            reply_set_location(request_id, protocol::SetLocationResult::SameServer, m_client_node->get_id());
            return;
        }

        auto proxy = static_cast<RemoteServerNode*>(new_server)->get_proxy();
        if (proxy == nullptr) {
            log(LOG_WARNING, "Found unknown region in the table: %s", new_server->get_range().to_string().c_str());
            reply_error(request_id, ENXIO);
            return;
        }
        auto self = shared_from_this();
        log(LOG_INFO, "Transfering client to %s", proxy->get_address().to_string().c_str());
        proxy->invoke_adopt_client([self, request_id, new_location, this](rpc::Error *err) {
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
            reply_set_location(request_id, protocol::SetLocationResult::DifferentServer,
                m_table->get_node_id_for_point(new_location));
        }, m_client_node->get_id(), m_client_node->get_coordinates(),
        m_client_node->get_address(), m_client_node->get_all_metadata());
    }

    virtual void handle_set_metadata(uint64_t request_id, std::string key, std::string value) override
    {
        check_client();

        if (m_client_node == nullptr)
            throw rpc::RemoteError(ENXIO);

        log(LOG_INFO, "Setting metadata key %s to \"%s\" for client %s", key.c_str(), value.c_str(),
            get_peer()->get_listening_address().to_string().c_str());
        m_client_node->set_metadata(key, value);
        reply_set_metadata(request_id);
    }

    virtual void handle_get_metadata(uint64_t request_id, NodeID node_id, std::string key) override
    {
        check_client();
        if (!node_id.is_valid())
            throw rpc::RemoteError(EINVAL);

        ClientNode *node = m_table->get_existing_client_node(node_id);
        if (node == nullptr) // this node ID is not here, call find_controlling_server() first
            throw rpc::RemoteError(ENOENT);

        log(LOG_INFO, "Get metadata request for key %s in client %s from %s", key.c_str(),
            node->get_id().to_string().c_str(), get_peer()->get_listening_address().to_string().c_str());
        reply_get_metadata(request_id, m_client_node->get_metadata(key));
    }

    virtual void handle_find_client_address(uint64_t request_id, NodeID node_id) override
    {
        check_client();
        if (!node_id.is_valid())
            throw rpc::RemoteError(EINVAL);

        ClientNode *node = m_table->get_existing_client_node(node_id);
        if (node == nullptr) // this node ID is not here or does not exist, call find_controlling_server() first
            throw rpc::RemoteError(ENOENT);

        assert(node->get_peer());
        reply_find_client_address(request_id, node->get_address());
    }

    virtual void handle_search_clients(uint64_t request_id, GeoPoint2D lower, GeoPoint2D upper) override
    {
        check_client_or_server();

        auto self = shared_from_this();
        m_table->search_clients(lower, upper, [self, request_id, this](rpc::Error* error, std::vector<NodeID>* reply) {
            if (error) {
                reply_error(request_id, EIO);
            } else {
                reply_search_clients(request_id, *reply);
            }
        });
    }
};

ServerContext::ServerContext(uv::Loop& loop, uint8_t resolution) :
    m_rpc(std::make_unique<rpc::Context>(loop)),
    m_table(std::make_unique<Table>(resolution))
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

    auto stub = std::dynamic_pointer_cast<ServerMasterImpl>(peer->get_stub(protocol::MASTER_OBJECT_ID));
    stub->register_server();

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
    if (m_peers.empty()) {
        // become the controller of the whole table
        m_table->add_local_server_node(NodeIDRange());
    } else {
        // register ourselves with the peers we know about
        for (auto address : m_peers)
            maybe_register_with_server(m_rpc.get(), address);
    }
}

}
