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

#include <cctype>
#include <exception>

namespace libhdht {

NodeID::NodeID()
{
    memset(m_parts, 0, sizeof(m_parts));
}

static inline uint8_t
hex_to_int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else
        return c - 'a';
}

NodeID::NodeID(const std::string& str)
{
    if (str.size() != sizeof(m_parts) * 2)
        throw std::invalid_argument("Invalid node ID");

    for (unsigned i = 0; i < sizeof(m_parts); i++) {
        char c1 = str[i*2];
        char c2 = str[i*2+1];

        if (!std::isxdigit(c1) || std::isxdigit(c2))
            throw std::invalid_argument("Invalid node ID");

        m_parts[i] = hex_to_int(c1) << 4 | hex_to_int(c2);
    }
}

NodeID::NodeID(const GeoPoint2D& point, uint8_t resolution)
{
    // TODO do something
    memset(m_parts, 0, sizeof(m_parts));
}

GeoPoint2D
NodeID::to_point() const
{
    // TODO: do something
    return GeoPoint2D{ 0, 0 };
}

bool
NodeID::has_mask(uint8_t mask) const
{
    assert(mask <= NodeID::size * 8);

    size_t i0 = mask / 8;
    size_t first_bit = mask % 8;
    uint8_t low_bits = ((1 << first_bit)-1) & 0xFF;
    if ((m_parts[i0] & low_bits) != 0)
        return false;

    for (size_t i = i0 + 1; i < NodeID::size; i++) {
        if (m_parts[i] != 0)
            return false;
    }

    return true;
}

std::string
NodeID::to_string() const
{
    std::string buffer;
    buffer.resize(8*NodeID::size);
    for (size_t i = 0; i < 8*NodeID::size; i++)
        buffer[i] = bit_at(i) ? '1' : '0';

    return buffer;
}

bool
NodeIDRange::contains(const NodeIDRange &subrange) const
{
    if (m_mask > subrange.m_mask)
        return false;

    return contains(subrange.m_from);
}

// returns a bit mask that has the highest num_bits set
static uint8_t high_bit_mask(size_t num_bits)
{
    uint8_t low_bits = ((1 << num_bits)-1) & 0xFF;
    return ~low_bits;
}

bool
NodeIDRange::contains(const NodeID & node) const
{
    size_t i;
    for (i = 0; i < m_mask / 8; i++) {
        if (m_from.m_parts[i] != node.m_parts[i])
            return false;
    }

    if (i * 8 < m_mask) {
        uint8_t mask = high_bit_mask(m_mask - i * 8);
        if ((m_from.m_parts[i] & mask) != (node.m_parts[i] & mask))
            return false;
    }
    return true;
}

bool
NodeIDRange::has_mask(uint8_t mask) const
{
    return m_mask <= mask && m_from.has_mask(m_mask);
}

std::string
NodeIDRange::to_string() const
{
    NodeID to = m_from;
    to.set_bit_at(m_mask, 1);

    return "from " + m_from.to_string() + " to " + to.to_string() + " (mask " + std::to_string(m_mask) + ")";
}


ServerNode::ServerNode(const NodeIDRange& id_range) : m_range(id_range)
{}


LocalServerNode::LocalServerNode(const NodeIDRange& range) : ServerNode(range)
{}

LocalServerNode *
LocalServerNode::split()
{
    LocalServerNode *new_node = new LocalServerNode(m_range);
    try {
        m_range.increase_mask();
        new_node->m_range.increase_mask();
        new_node->m_range.from().set_bit_at(m_range.mask(), 1);

        std::unordered_set<ClientNode*> left, right;
        for (ClientNode* client : m_clients) {
            if (client->get_id().bit_at(m_range.mask()-1))
                right.insert(client);
            else
                left.insert(client);
        }
        std::swap(m_clients, left);
        std::swap(new_node->m_clients, right);

        return new_node;
    } catch(const std::bad_alloc& e) {
        delete new_node;
        throw;
    }
}

void
LocalServerNode::adopt_nodes(LocalServerNode *from)
{
    m_clients.insert(from->m_clients.begin(), from->m_clients.end());
}


RemoteServerNode::RemoteServerNode(const NodeIDRange& range, std::shared_ptr<protocol::ServerProxy> proxy) :
    ServerNode(range), m_proxy(proxy)
{}

RemoteServerNode *
RemoteServerNode::split()
{
    RemoteServerNode *new_node = new RemoteServerNode(m_range, m_proxy);
    m_range.increase_mask();
    new_node->m_range.increase_mask();
    new_node->m_range.from().set_bit_at(m_range.mask(), 1);

    return new_node;
}

}
