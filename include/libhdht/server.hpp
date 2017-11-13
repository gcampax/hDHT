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

#include "net.hpp"
#include "server-node.hpp"
#include "client-node.hpp"
#include "dht.hpp"
#include "uv.hpp"
#include "rpc.hpp"

namespace libhdht {

// The context for a single server instance of libhdht
class ServerContext
{
private:
    rpc::Context m_rpc;
    Table m_table;
    std::vector<ServerNode*> m_server_nodes;
    std::vector<net::Address> m_peers;

public:
    ServerContext(uv::Loop& loop);
    ~ServerContext() {}

    // expose this server on this address
    void add_address(const net::Address& address);

    // add the given peer as known in the table
    void add_peer(const net::Address& address);

    // register this server in the DHT
    // (must have at least one peer in the table)
    void start();
};

}
