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
#include "uv.hpp"

namespace libhdht
{

// forward declarations of private classes
class ServerMasterImpl;
class Table;
namespace rpc
{
    class Context;
}

// The context for a single server instance of libhdht
class ServerContext
{
    friend class ServerMasterImpl;

private:
    std::unique_ptr<rpc::Context> m_rpc;
    std::vector<net::Address> m_peers;
    std::unique_ptr<Table> m_table;

public:
    ServerContext(uv::Loop& loop);
    ~ServerContext();

    ServerContext(const ServerContext&) = delete;
    ServerContext(ServerContext&&) = delete;
    ServerContext& operator=(const ServerContext&) = delete;
    ServerContext& operator=(ServerContext&&) = delete;

    // expose this server on this address
    void add_address(const net::Address& address);

    // add the given peer as known in the table
    void add_peer(const net::Address& address);

    // register this server in the DHT
    // (must have at least one peer in the table)
    void start();
};

}
