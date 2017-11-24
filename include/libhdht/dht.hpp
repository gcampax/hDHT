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
#include <cassert>

#include "node.hpp"
#include "client-node.hpp"
#include "server-node.hpp"

namespace libhdht {

// the actual table, holds pointers to all the nodes, and is responsible
// for freeing them
class Table
{
    std::map<NodeID, Node*> m_known_nodes;

    uint8_t m_resolution;

public:
    Table(uint8_t resolution)
    {
        // there is more than one client
        assert(resolution > 0);
        // we can represent nodes in that many bytes
        assert(resolution <= NodeID::size * 8);
    }
    ~Table();

    // the log order of the Hilber curve (= the resolution of the grid, at the finest level)
    // this is the log of the maximum number of clients
    uint64_t resolution() const
    {
        return m_resolution;
    }

    bool is_valid_node_id(const NodeID& id) const
    {
        return id.has_mask(m_resolution);
    }

    NodeID get_node_id_for_point(GeoPoint2D& pt) const
    {
        return NodeID(pt, m_resolution);
    }

    Node *get_existing_node(const NodeID&) const;
    LocalClientNode *get_or_create_local_client_node(const NodeID&);
    LocalServerNode *get_or_create_local_server_node(const NodeID&);

    RemoteServerNode *get_or_create_remote_server_node(const NodeID&);
    RemoteClientNode *get_or_create_remote_client_node(const NodeID&);
};

}
