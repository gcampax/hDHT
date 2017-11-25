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

#include "libhdht-private.hpp"

namespace libhdht {

Table::~Table()
{
    for (auto& node : m_ranges)
        delete node;
}

void
Table::add_remote_server_node(const NodeIDRange& range, std::shared_ptr<protocol::ServerProxy> proxy)
{

}

void
Table::add_local_server_node(const NodeIDRange& range)
{

}

ClientNode*
Table::get_or_create_client_node(const GeoPoint2D &pt)
{
    return nullptr;
}

ClientNode*
Table::get_or_create_client_node(const NodeID &id)
{
    return nullptr;
}

bool
Table::move_client(ClientNode* node, const GeoPoint2D & pt, RemoteServerNode *& new_server)
{
    new_server = nullptr;
    return true;
}

void
Table::forget_client(ClientNode* node)
{
}

void
Table::find_controlling_server(const NodeID &id, libhdht::ServerNode *&node, NodeIDRange & range) const
{
    node = nullptr;
}

}
