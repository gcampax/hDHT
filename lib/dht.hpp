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
#include <map>
#include <list>

#include "node.hpp"

namespace libhdht {

// the actual table, holds pointers to all the nodes, and is responsible
// for freeing them
class Table
{
    uint8_t m_resolution;
    // all the different chunks in the DHT
    // Initially, the whole DHT is owned by the local server
    // As the server discovers more peers it will split the ranges more
    // and more finely
    std::map<NodeID, ServerNode*> m_ranges;

    // the currently connected clients, indexed by their node ID
    std::map<NodeID, ClientNode*> m_clients;

public:
    Table(uint8_t resolution);
    ~Table();

    // the log order of the Hilber curve (= the resolution of the grid, at the finest level)
    // this is the log of the maximum number of clients
    uint64_t resolution() const
    {
        return m_resolution;
    }

    bool is_valid_range(const NodeIDRange& id) const
    {
        return id.has_mask(m_resolution);
    }

    NodeID get_node_id_for_point(const GeoPoint2D& pt) const
    {
        return NodeID(pt, m_resolution);
    }

    // Client management
    ClientNode *get_or_create_client_node(const NodeID& id, const GeoPoint2D& pt);
    ClientNode *get_existing_client_node(const NodeID& id);
    ServerNode *move_client(ClientNode *, const GeoPoint2D&);
    void forget_client(ClientNode *);
    void forget_server(ServerNode *);

    // Server (range) management
    bool add_remote_server_node(const NodeIDRange& range, std::shared_ptr<protocol::ServerProxy> proxy);
    void add_local_server_node(const NodeIDRange& range, LocalServerNode *existing = nullptr);
    ServerNode *find_controlling_server(const NodeID& id) const;

    // perform any load balancing by splitting any local range
    enum class LoadBalanceAction {
        InformPeer,
        RelinquishRange
    };
    void load_balance_with_peer(std::shared_ptr<protocol::ServerProxy>,
        std::function<void(LoadBalanceAction, ServerNode*)>);

    // dump the table to the log (with level LOG_DEBUG)
    void debug_dump_table() const;

};

}
