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
NodeIDRange::contains(const NodeIDRange &subrange) const
{
    if (m_log_size > subrange.m_log_size)
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
    for (i = 0; i < m_log_size / 8; i++) {
        if (m_from.m_parts[i] != node.m_parts[i])
            return false;
    }

    if (i * 8 < m_log_size) {
        uint8_t mask = high_bit_mask(m_log_size - i * 8);
        if ((m_from.m_parts[i] & mask) != (node.m_parts[i] & mask))
            return false;
    }
    return true;
}

}
