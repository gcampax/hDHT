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

#include "net.hpp"
#include "node.hpp"

namespace libhdht {

// A client (ie, a mobile phone with a real-world location) in the DHT
class ClientNode : public Node
{
    std::unordered_map<std::string, std::string> m_metadata;

public:
    ClientNode(const NodeID& id) : Node(id) {}
    ~ClientNode() {}

    const std::string& get_metadata(const std::string key) const;

    bool is_client() const override
    {
        return true;
    }
};

// A client node that is running in this library/process
class LocalClientNode : public ClientNode
{
public:
    LocalClientNode(const NodeID& id) : ClientNode(id) {}

    bool is_local() const override
    {
        return true;
    }
};

// A client node that is not running in this process (and therefore is behind RPC)
class RemoteClientNode : public ClientNode
{
public:
    RemoteClientNode(const NodeID& id) : ClientNode(id) {}

    bool is_local() const override
    {
        return false;
    }
};


}
