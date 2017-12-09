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

#include <cstdlib>
#include <algorithm>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <map>

#include "net.hpp"
#include "geo.hpp"
#include "uv.hpp"

namespace libhdht
{

// forward declarations of private classes
class ClientMasterImpl;
namespace rpc
{
    class Context;
    class Peer;
}

// The context for a single client instance of libhdht
class ClientContext
{
    friend class ClientMasterImpl;

private:
    std::unique_ptr<rpc::Context> m_rpc;
    net::Address m_initial_server;
    std::shared_ptr<rpc::Peer> m_current_server;
    // cache mapping from other client nodes to the server responsible for them
    mutable std::map<NodeID, std::shared_ptr<rpc::Peer>> m_other_client_cache;

    GeoPoint2D m_coordinates;
    mutable std::unordered_map<std::string, std::string> m_metadata;
    NodeID m_node_id;
    // was_registered: was this client ever registered to any server
    bool m_was_registered = false;
    // is_registered: is this client registered to m_current_server
    // (ie, can it call set_location()
    bool m_is_registered = false;
    bool m_is_updating_location = false;
    int m_registration_retry_counter = 0;
    // must_set_location: if true, the servers are not yet aware of the
    // latest location change of this client
    bool m_must_set_location = false;
    std::unordered_map<std::string, std::string> m_pending_metadata_changes;

    void update_current_server();
    void continue_registration();
    void do_register();
    void do_set_location();
    void do_set_one_metadata(const std::string& key, const std::string& value);

    enum class MetadataFlushMode {
        Everything,
        OnlyChanges
    };
    void flush_metadata_changes(MetadataFlushMode mode);

    void do_get_remote_metadata(const NodeID&, const std::string& key,
        std::function<void(rpc::Error*, const std::string*)> callback, bool retry) const;

public:
    ClientContext(uv::Loop& loop);
    virtual ~ClientContext();

    ClientContext(const ClientContext&) = delete;
    ClientContext(ClientContext&&) = delete;
    ClientContext& operator=(const ClientContext&) = delete;
    ClientContext& operator=(ClientContext&&) = delete;

    // listen on this address
    void add_address(const net::Address& address);

    // set the given peer as the initial server to contact
    void set_initial_server(const net::Address& address);

    void set_location(const GeoPoint2D& point);
    const GeoPoint2D get_location() const
    {
        return m_coordinates;
    }

    void set_local_metadata(const std::string& key, const std::string& value);
    const std::string& get_local_metadata(const std::string& key) const
    {
        return m_metadata[key];
    }

    void get_remote_metadata(const NodeID&, const std::string& key,
        std::function<void(rpc::Error*, const std::string*)> callback) const;

    void search_clients(const GeoPoint2D& upper, const GeoPoint2D& lower,
        std::function<void(rpc::Error*, const std::vector<NodeID>)> callback) const;

    net::Address get_current_server() const;
    const NodeID& get_current_node_id() const
    {
        return m_node_id;
    }

    virtual void on_register() {};
};

}
