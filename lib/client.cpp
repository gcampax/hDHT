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

class RemoteClient
{
private:
    std::shared_ptr<protocol::ServerProxy> m_owner_server;
    NodeID m_node_id;

public:
    RemoteClient(std::shared_ptr<protocol::ServerProxy>, const NodeID&);
    ~RemoteClient();

    net::Address get_address() const;

    void get_metadata(const std::string& key,
        const std::function<void(rpc::Error*, const std::string*)>& callback);
};


ClientContext::ClientContext(uv::Loop& loop) :
    m_rpc(std::make_unique<rpc::Context>(loop))
{}

ClientContext::~ClientContext()
{}

net::Address
ClientContext::get_current_server() const
{
    return m_current_server->get_listening_address();
}

void
ClientContext::add_address(const net::Address &address)
{
    m_rpc->add_address(address);
}

void
ClientContext::set_initial_server(const net::Address &address)
{
    m_current_server = m_rpc->get_peer(address);
}

void
ClientContext::set_location(const libhdht::GeoPoint2D &point)
{
    m_coordinates = point;
    m_must_set_location = true;

    if (m_is_registered)
        do_set_location();
    else if (!m_was_registered)
        do_register();

    // if !m_is_registered and m_was_registered
    // we're in the process of migrating to a new server
    // the servers take care of most of it, we just need to find out
    // where it is
    // as soon as update_current_server() is done, it will see
    // m_must_set_location == true and call do_set_location
}

void
ClientContext::do_set_location()
{
    // if you call set_location and are not registered you get EPERM, which is bad
    assert(m_is_registered);

    auto proxy = m_current_server->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);

    proxy->invoke_set_location([this](rpc::Error *err, protocol::SetLocationResult result, NodeID new_node_id) {
        if (err) {
            rpc::RemoteError *remote_err = dynamic_cast<rpc::RemoteError*>(err);
            if (remote_err) {
                switch (remote_err->code()) {
                case ENXIO:
                    // the DHT was rebalanced, or something else weird occurred
                    m_is_registered = false;
                    update_current_server();
                    return;

                default:
                    // fallthrough
                    ;
                }
            }

            log(LOG_WARNING, "Failed to set client location: %s", err->what());

            // reset everything
            m_is_registered = false;
            m_was_registered = false;
            do_register();
            return;
        }

        // location was set successfully, even if the client was migrated to a different
        // server
        m_must_set_location = false;
        m_node_id = new_node_id;
        if (result == protocol::SetLocationResult::DifferentServer) {
            m_is_registered = false;
            update_current_server();
        }
    }, m_coordinates);
}

void
ClientContext::update_current_server()
{
    assert(!m_is_registered);

    // the concurrent request will flush any changes we need
    if (m_is_updating_location)
        return;

    m_is_updating_location = true;
    auto proxy = m_current_server->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);

    proxy->invoke_find_controlling_server([this](rpc::Error* err, net::Address server_address, const NodeIDRange&) {
        if (err) {
            log(LOG_WARNING, "Failed to find own controlling server: %s", err->what());

            // reset everything
            m_is_registered = false;
            m_was_registered = false;
            do_register();
            return;
        }

        // the client does not care for the node ID range,
        // the DHT is maintained entirely by the servers
        m_current_server = m_rpc->get_peer(server_address);
        m_is_updating_location = false;

        // we must register with this server regardless of what the server knows about us
        // so that it will treat our connection as a client connection and not reject our RPC
        do_register();
    }, m_node_id);
}

void
ClientContext::do_register()
{
    assert(!m_is_registered);
    assert(!m_is_updating_location);

    // if we were ever registered, try to register again with the old coordinates
    // which will prevent us from having to reupload all the metadata anew
    GeoPoint2D coordinates = m_was_registered ? m_node_id.to_point() : m_coordinates;

    auto proxy = m_current_server->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);
    proxy->invoke_client_hello([this](rpc::Error* err, protocol::ClientRegistrationResult result, NodeID node_id) {
        if (err) {
            log(LOG_WARNING, "Failed to register with server: %s", err->what());

            // reset everything
            m_is_registered = false;
            m_was_registered = false;

            if (++m_registration_retry_counter <= 5)
                do_register();
            else
                log(LOG_ERR, "Too many registration failures! Aborting...");
            return;
        }

        if (result == protocol::ClientRegistrationResult::WrongServer) {
            // we got bounced around again...
            update_current_server();
            return;
        }

        m_node_id = node_id;
        m_is_registered = true;

        if (!m_was_registered || result == protocol::ClientRegistrationResult::ClientCreated) {
            m_was_registered = true;
            m_must_set_location = false;
            continue_registration();
        } else {
            // trust that servers don't misbehave too badly
            assert(result == protocol::ClientRegistrationResult::ClientAlreadyExists);

            if (m_must_set_location)
                do_set_location();
            flush_metadata_changes(MetadataFlushMode::OnlyChanges);
        }
    }, m_rpc->get_listening_address(), coordinates);
}

void
ClientContext::continue_registration()
{
    flush_metadata_changes(MetadataFlushMode::Everything);
}

void
ClientContext::set_local_metadata(const std::string &key, const std::string &value)
{
    m_metadata.insert(std::make_pair(key, value));

    if (m_was_registered)
        m_pending_metadata_changes.insert(std::make_pair(key, value));

    if (m_is_registered)
        flush_metadata_changes(MetadataFlushMode::OnlyChanges);
}

void
ClientContext::flush_metadata_changes(ClientContext::MetadataFlushMode mode)
{
    if (mode == MetadataFlushMode::Everything) {
        m_pending_metadata_changes.clear();

        for (const auto& entry : m_metadata) {
            m_pending_metadata_changes[entry.first] = entry.second;
            do_set_one_metadata(entry.first, entry.second);
        }
    } else {
        for (const auto& entry : m_pending_metadata_changes)
            do_set_one_metadata(entry.first, entry.second);
    }
}

void
ClientContext::do_set_one_metadata(const std::string& key, const std::string& value)
{
    // if you call set_metadata and are not registered you get EPERM, which is bad
    assert(m_is_registered);

    auto proxy = m_current_server->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);

    proxy->invoke_set_metadata([this, key](rpc::Error *err) {
        if (err) {
            rpc::RemoteError *remote_err = dynamic_cast<rpc::RemoteError*>(err);
            if (remote_err) {
                switch (remote_err->code()) {
                case ENXIO:
                    // the DHT was rebalanced, or something else weird occurred
                    m_is_registered = false;
                    update_current_server();
                    return;

                default:
                    // fallthrough
                    ;
                }
            }

            log(LOG_WARNING, "Failed to set metadata: %s", err->what());

            // reset everything
            m_is_registered = false;
            m_was_registered = false;
            do_register();
            return;
        }

        m_pending_metadata_changes.erase(key);
    }, key, value);
}

void
ClientContext::do_get_remote_metadata(const NodeID &node_id, const std::string &key, std::function<void(rpc::Error*, const std::string*)> callback, bool retry) const
{
    auto cached_server = m_other_client_cache[node_id];
    if (cached_server) {
        auto proxy = cached_server->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);

        proxy->invoke_get_metadata([=](rpc::Error *err, const std::string& value) {
            if (err) {
                rpc::RemoteError *remote_err = dynamic_cast<rpc::RemoteError*>(err);
                if (remote_err) {
                    switch (remote_err->code()) {
                    case ENOENT:
                        if (retry) {
                            // our cache was stale
                            m_other_client_cache.erase(node_id);
                            do_get_remote_metadata(node_id, key, std::move(callback), true);
                            return;
                        }
                        // else the client with that NodeID disappeared or moved
                        // fallthrough and report an error to the caller
                    default:
                        // fallthrough
                        ;
                    }
                }

                callback(err, nullptr);
                return;
            } else {
                callback(nullptr, &value);
            }
        }, node_id, key);
    } else {
        auto proxy = m_current_server->get_proxy<protocol::ServerProxy>(protocol::MASTER_OBJECT_ID);

        proxy->invoke_find_controlling_server([=](rpc::Error *err, net::Address server_address, const NodeIDRange&) {
            if (err) {
                callback(err, nullptr);
                return;
            } else {
                m_other_client_cache[node_id] = m_rpc->get_peer(server_address);

                // set retry flag to false so we don't loop again to try and lookup the server
                // if the server answers ENOENT we know that this client really does not exist
                do_get_remote_metadata (node_id, key, std::move(callback), false);
            }
        }, node_id);
    }
}

void
ClientContext::get_remote_metadata(const NodeID &node_id, const std::string &key, std::function<void(rpc::Error*, const std::string*)> callback) const
{
    do_get_remote_metadata(node_id, key, std::move(callback), true);
}

}
