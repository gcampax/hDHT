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
#include "hilbert-values.hpp"
#include "endian.hpp"

#include <cctype>
#include <exception>

namespace libhdht {

NodeID::NodeID()
{
    memset(m_parts, 0, sizeof(m_parts));
    assert(!is_valid());
}

static inline uint8_t
hex_to_int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else
        return 10 + c - 'a';
}

NodeID::NodeID(const std::string& str)
{
    if (str.size() != sizeof(m_parts) * 2)
        throw std::invalid_argument("Invalid node ID");

    for (unsigned i = 0; i < sizeof(m_parts); i++) {
        char c1 = str[i*2];
        char c2 = str[i*2+1];

        if (!std::isxdigit(c1) || !std::isxdigit(c2))
            throw std::invalid_argument("Invalid node ID");

        m_parts[i] = hex_to_int(c1) << 4 | hex_to_int(c2);
    }

    assert(is_valid() || is_all_zeros());
}

NodeID::NodeID(const GeoPoint2D& point, uint8_t resolution)
{
    std::pair<uint64_t, uint64_t> fixed_point = point.to_fixed_point();
    assert(resolution <= 104);

    // resolution is the resolution of the hilbert curve, so
    // the resolution of the grid is half of that
    uint64_t shift = (64 - resolution);
    uint64_t n = (1ULL << (resolution/2));
    uint64_t d = hilbert_values::xy2d(n, fixed_point.first >> (64 - resolution / 2), fixed_point.second >> (64 - resolution / 2));

    d <<= shift;
    d = htobe64(d);
    memcpy(m_parts, &d, sizeof(d));
    memset(m_parts + sizeof(d), 0, sizeof(m_parts) - sizeof(d));

    set_valid();
}

NodeID::NodeID(uint64_t hilbert_value, uint8_t resolution)
{
    // resolution is the resolution of the hilbert curve, so
    // the resolution of the grid is half of that
    uint64_t shift = (64 - resolution);

    hilbert_value <<= shift;
    hilbert_value = htobe64(hilbert_value);
    memcpy(m_parts, &hilbert_value, sizeof(hilbert_value));
    memset(m_parts + sizeof(hilbert_value), 0, sizeof(m_parts) - sizeof(hilbert_value));

    set_valid();
}

std::pair<uint64_t, uint64_t>
NodeID::to_point(uint8_t resolution) const
{
    assert(resolution <= 64);

    // resolution is the resolution of the hilbert curve, so
    // the resolution of the grid is half of that

    uint64_t x, y;
    uint64_t shift = (64 - resolution);
    uint64_t n = (1ULL << (resolution/2));
    uint64_t d = to_hilbert_value(resolution);
    hilbert_values::d2xy(n, d, x, y);
    x <<= shift;
    y <<= shift;

    return std::make_pair(x, y);
}

uint64_t
NodeID::to_hilbert_value(uint8_t resolution) const
{
    uint64_t shift = (64 - resolution);
    uint64_t d;
    memcpy(&d, m_parts, sizeof(d));
    d = be64toh(d);
    d >>= shift;
    return d;
}

bool
NodeID::has_mask(uint8_t mask) const
{
    assert(mask <= NodeID::size * 8);
    if (mask == NodeID::size * 8)
        return true;

    size_t i0 = mask / 8;
    size_t first_bit = 7 - mask % 8;
    uint8_t low_bits = ((1 << first_bit)-1) & 0xFF;
    if ((m_parts[i0] & low_bits) != 0)
        return false;

    for (size_t i = i0 + 1; i < NodeID::size; i++) {
        if (m_parts[i] != 0)
            return false;
    }

    return true;
}

bool
NodeID::is_all_zeros() const
{
    for (size_t i = 0; i < sizeof(m_parts); i++) {
        if (m_parts[i] != 0)
            return false;
    }
    return true;
}

std::string
NodeID::to_string() const
{
    std::string buffer;
    /*
    buffer.resize(8*NodeID::size);
    for (size_t i = 0; i < 8*NodeID::size; i++)
        buffer[i] = bit_at(i) ? '1' : '0';
    */
    // only print the first 32 bits
    buffer.resize(32);
    for (size_t i = 0; i < 32; i++)
        buffer[i] = bit_at(i) ? '1' : '0';

    return buffer;
}

std::string
NodeID::to_hex() const
{
    std::string str;
    str.resize(NodeID::size * 2);

    for (size_t i = 0; i < NodeID::size; i++) {
        uint8_t c = m_parts[i];
        str[2*i] = (c >> 4) <= 9 ? ('0' + (c >> 4)) : ('a' + ((c >> 4) - 10));
        str[2*i+1] = (c & 0xf) <= 9 ? ('0' + (c & 0xf)) : ('a' + ((c & 0xf) - 10));
    }

    return str;
}

bool
NodeIDRange::contains(const NodeIDRange &subrange) const
{
    if (m_mask > subrange.m_mask)
        return false;

    return contains(subrange.m_from);
}

NodeID
NodeIDRange::to() const
{
    NodeID to = from();
    for (uint8_t i = m_mask; i < NodeID::size*8; i++)
        to.set_bit_at(i, 1);

    return to;
}

// returns a 8-bit mask that has the highest num_bits set
static uint8_t high_bit_mask(size_t num_bits)
{
    uint8_t low_bits = ((1 << (8 - num_bits))-1) & 0xFF;
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

    if (m_mask % 8) {
        uint8_t mask = high_bit_mask(m_mask % 8);
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
    for (uint8_t i = m_mask; i < NodeID::size*8; i++)
        to.set_bit_at(i, 1);

    return "from " + m_from.to_string() + " to " + to.to_string() + " (mask " + std::to_string(m_mask) + ")";
}

ServerNode::ServerNode(const NodeIDRange& id_range) : m_range(id_range)
{}


LocalServerNode::LocalServerNode(const NodeIDRange& range, uint8_t resolution)
    : ServerNode(range), m_clients(1ULL << (resolution/2)), m_resolution(resolution)
{}

LocalServerNode *
LocalServerNode::split()
{
    LocalServerNode *new_node = new LocalServerNode(m_range, m_resolution);
    try {
        m_range.increase_mask();
        new_node->m_range.increase_mask();
        new_node->m_range.from().set_bit_at(m_range.mask()-1, 1);

        rtree::RTree left(1ULL << (m_resolution/2)), right(1ULL << (m_resolution/2));
        m_clients.foreach_entry([&left, &right, this](const std::shared_ptr<rtree::LeafEntry>& entry) -> void {
            ClientNode *client = static_cast<ClientNode*>(entry->get_data());
            auto pt = client->get_id().to_point(m_resolution);
            if (client->get_id().bit_at(m_range.mask()-1))
                right.insert(pt, client);
            else
                left.insert(pt, client);
        });
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
    from->foreach_client([this](ClientNode *client) {
        add_client(client);
    });
}

void
LocalServerNode::add_client(ClientNode *client)
{
    auto pt = client->get_id().to_point(m_resolution);
    m_clients.insert(pt, client);
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
    new_node->m_range.from().set_bit_at(m_range.mask()-1, 1);

    return new_node;
}

}
