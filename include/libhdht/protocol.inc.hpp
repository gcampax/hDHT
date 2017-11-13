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

// NOTE: this is NOT "pragma once"
// this file is included multiple times, defining different values
// for the macros, to put the protocol definition in various places

// Note: you cannot have requests with no parameters (due to how the
// preprocessors expands __VA_ARGS__ macros, it would create a dangling comma)

begin_class(Master)
    // server_hello: a server contacts another server to bootstrap the protocol
    request(void, server_hello, std::shared_ptr<MasterProxy>)

    // register_server_node: a virtual server node joins the DHT
    request(void, register_server_node, NodeID, std::shared_ptr<ServerNodeProxy>)

    // register_client_node: a client node joins the DHT
    request(void, register_client_node, double, double, std::shared_ptr<ClientNodeProxy>)
end_class

begin_class(ServerNode)
end_class

begin_class(ClientNode)
end_class
