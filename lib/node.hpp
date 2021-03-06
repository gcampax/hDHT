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
#include <cstdint>
#include <cstring>
#include <string>
#include <cassert>

#include <libhdht/geo.hpp>
#include <libhdht/net.hpp>
#include "protocol.hpp"

#include "rtree/rtree.hpp"

namespace libhdht {

// A client (ie, a mobile phone with a real-world location) in the DHT
class ClientNode
{
    std::shared_ptr<rpc::Peer> m_peer;
    NodeID m_node_id;
    GeoPoint2D m_coordinates;
    mutable std::unordered_map<std::string, std::string> m_metadata;
    bool m_registered = false;

public:
    ClientNode(const NodeID& id, const GeoPoint2D& coordinates) : m_node_id(id), m_coordinates(coordinates) {}
    ~ClientNode() {}

    bool is_registered() const
    {
        return m_registered;
    }

    void set_registered()
    {
        m_registered = true;
    }

    const NodeID& get_id()
    {
        return m_node_id;
    }
    void set_id(NodeID id)
    {
        m_node_id = id;
    }
    const GeoPoint2D& get_coordinates() const
    {
        return m_coordinates;
    }
    void set_coordinates(const GeoPoint2D& coordinates)
    {
        m_coordinates = coordinates;
    }

    const std::unordered_map<std::string, std::string> get_all_metadata() const
    {
        return m_metadata;
    }
    void set_all_metadata(std::unordered_map<std::string, std::string>&& metadata)
    {
        m_metadata = std::move(metadata);
    }
    const std::string& get_metadata(const std::string& key) const
    {
        return m_metadata[key];
    }
    void set_metadata(const std::string& key, const std::string& value)
    {
        m_metadata[key] = value;
    }

    net::Address get_address() const
    {
        return m_peer->get_listening_address();
    }
    std::shared_ptr<rpc::Peer> get_peer() const
    {
        return m_peer;
    }
    void set_peer(std::shared_ptr<rpc::Peer> peer)
    {
        m_peer = peer;
    }
};

// A node in the DHT that acts as a server (ie, a range of NodeIDs that know about
// a certain list of clients)
class ServerNode
{
protected:
    NodeIDRange m_range;
    bool m_frozen;

public:
    ServerNode(const NodeIDRange& id_range);
    virtual ~ServerNode() {}

    virtual ServerNode* split() = 0;
    virtual bool is_local() const = 0;

    const NodeIDRange& get_range() const
    {
        return m_range;
    }
    void set_range(NodeIDRange range)
    {
        m_range = range;
    }

    void freeze()
    {
        m_frozen = true;
    }
    void thaw()
    {
        m_frozen = false;
    }
};

// A server node owned by this library/process
class LocalServerNode : public ServerNode
{
    // the clients that are registered with this server
    rtree::RTree m_clients;
    uint8_t m_resolution;

public:
    LocalServerNode(const NodeIDRange& id, uint8_t resolution);

    virtual LocalServerNode* split() override;

    virtual bool is_local() const override
    {
        return true;
    }

    size_t load() const
    {
        return m_clients.size();
    }

    void adopt_nodes(LocalServerNode* from);

    void prepare_insert()
    {
        // TODO
    }
    void add_client(ClientNode *client);
    void remove_client(ClientNode *client)
    {
        // TODO
    }

    std::vector<std::shared_ptr<rtree::LeafEntry>> search(const rtree::Rectangle& rect) const
    {
        return m_clients.search(rect);
    }

    template<typename Callback>
    void foreach_client(const Callback& callback) const
    {
        m_clients.foreach_entry([&callback](std::shared_ptr<rtree::LeafEntry> entry) {
            callback(static_cast<ClientNode*>(entry->get_data()));
        });
    }
};

// A server node somewhere else in the world
class RemoteServerNode : public ServerNode
{
    std::shared_ptr<protocol::ServerProxy> m_proxy;

public:
    RemoteServerNode(const NodeIDRange& id, std::shared_ptr<protocol::ServerProxy> proxy);

    std::shared_ptr<protocol::ServerProxy> get_proxy() const
    {
        return m_proxy;
    }
    void set_proxy(std::shared_ptr<protocol::ServerProxy> proxy)
    {
        m_proxy = proxy;
    }

    virtual RemoteServerNode* split() override;

    bool is_local() const override
    {
        return false;
    }
};

}
