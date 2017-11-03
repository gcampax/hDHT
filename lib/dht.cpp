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

#include <libhdht/libhdht.hpp>

namespace libhdht {

Table::~Table()
{
    for (auto& node : m_known_nodes)
        delete node.second;
}

Node *
Table::get_existing_node(const NodeID& id) const
{
    const auto& iterator = m_known_nodes.find(id);
    if (iterator != m_known_nodes.end())
        return iterator->second;
    else
        return nullptr;
}

LocalClientNode *
Table::get_or_create_local_client_node(const NodeID& id)
{
    const auto& iterator = m_known_nodes.find(id);
    if (iterator != m_known_nodes.end())
        return dynamic_cast<LocalClientNode*>(iterator->second);

    LocalClientNode *node = new LocalClientNode(id);
    m_known_nodes.insert(std::make_pair(id, node));
    return node;
}

RemoteClientNode *
Table::get_or_create_remote_client_node(const NodeID& id)
{
    const auto& iterator = m_known_nodes.find(id);
    if (iterator != m_known_nodes.end())
        return dynamic_cast<RemoteClientNode*>(iterator->second);

    RemoteClientNode *node = new RemoteClientNode(id);
    m_known_nodes.insert(std::make_pair(id, node));
    return node;
}

LocalServerNode *
Table::get_or_create_local_server_node(const NodeID& id)
{
    const auto& iterator = m_known_nodes.find(id);
    if (iterator != m_known_nodes.end())
        return dynamic_cast<LocalServerNode*>(iterator->second);

    LocalServerNode *node = new LocalServerNode(id);
    m_known_nodes.insert(std::make_pair(id, node));
    return node;
}

RemoteServerNode *
Table::get_or_create_remote_server_node(const NodeID& id)
{
    const auto& iterator = m_known_nodes.find(id);
    if (iterator != m_known_nodes.end())
        return dynamic_cast<RemoteServerNode*>(iterator->second);

    RemoteServerNode *node = new RemoteServerNode(id);
    m_known_nodes.insert(std::make_pair(id, node));
    return node;
}

}
