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

#include "node.hpp"

namespace libhdht {

// A node in the DHT that acts as a server
class ServerNode : public Node
{
public:
    ServerNode(const NodeID& id) : Node(id) {}
    ~ServerNode() {}

    bool is_client() const override
    {
        return false;
    }
};

// A server node owned by this library/process
class LocalServerNode : public ServerNode
{
    // the clients that are registered with this server
    std::vector<Node*> m_clients;

    // the immediate successor in the DHT
    ServerNode *next = nullptr;

public:
    LocalServerNode(const NodeID& id) : ServerNode(id) {}

    bool is_local() const override
    {
        return true;
    }
};

// A server node somewhere else in the world
class RemoteServerNode : public ServerNode
{
public:
    RemoteServerNode(const NodeID& id) : ServerNode(id) {}

    bool is_local() const override
    {
        return false;
    }
};

}
