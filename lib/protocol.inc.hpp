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

// Note 1: all methods must have distinct names, because the names end up
// in a single Opcode enum

// Note 2: you cannot have requests with no parameters (due to how the
// preprocessors expands __VA_ARGS__ macros, it would create a dangling comma)
// pass a dummy integer if you really don't want any parameters

// Note 3: to pass around RPC objects, you must declare a std::shared_ptr<> of the proxy type
// This is automatically converted to the stub type in the declaration of the proxy
// containing that request

// A single Server (which can serve many virtual ServerNodes)
// An instance of this object is always available at object id 1
// The protocol initiates with each peer sending an hello() to another Server, of which it
// knows the address
begin_class(Server)
    // server_hello: a server contacts another server to bootstrap the protocol
    // the first and only argument is the "primary address" of the server (a routable address,
    // with an open port, unlike the peer address which would use some random high port)
    // the server who performs the request must be listening on the given address and
    // must export a Server object at object id 1
    request(void, server_hello, net::Address)

    // client_hello: a client contacts a server to bootstrap the protocol
    // the client must be listening on the given address and export a Client object at
    // object id 1
    // the passed point is the previous node ID, if any, or the all zero node id
    // the passed point is the initial location of the client
    // returns true if the client was successfully registered with this server, false
    // if the client needs to go and find the server responsible for this client (calling
    // find_controlling_server) and register again
    request(ClientRegistrationReply, client_hello, net::Address, NodeID, GeoPoint2D)

    // add_remote_range: learn about this range, owned by another server (either the responding
    // one or a third party)
    // this is called by a server to a new server as part of onboarding
    request(void, add_remote_range, NodeIDRange, net::Address)

    // control_range: become the controlling server for this range
    // this is called by a server to a new server as part of onboarding, or as part of load
    // balancing
    request(void, control_range, NodeIDRange)

    // adopt_client: adopt a client that was already registered
    // this is called by a server to a new server immediately after transferring control
    // of a range
    request(void, adopt_client, NodeID, GeoPoint2D, net::Address, MetadataType)

    // find_controlling_server: find the address of the server that controls the
    // range containing this NodeID
    // returns the address of the server and the range (which a server would use
    // to refine its own DHT)
    // this is called by a client or server
    request(AddressAndRange, find_controlling_server, NodeID)

    // find_server_for_point: find the address of the server that controls the
    // range containing this point
    // returns the address of the server and the range (which a server would use
    // to refine its own DHT)
    // this is called by a client or server
    request(AddressAndRange, find_server_for_point, GeoPoint2D)

    // set the physical location of the calling client
    // this is called by a client only
    // the request returns whether if the client is still controlled by the same server
    // if the request fails the client should assume to be in a limbo state and start the
    // registration process from scratch
    request(SetLocationReply, set_location, GeoPoint2D)

    // set the physical location of the calling client
    // this is called by a client only
    request(void, set_metadata, std::string, std::string)

    request(net::Address, find_client_address, NodeID)

    request(std::string, get_metadata, NodeID, std::string)

    // search_clients: find all clients that are registered in the DHT in this
    // rectangle
    //request(void, search_clients, Rectangle, std::vector<NodeID>)
end_class

begin_class(Client)
end_class
