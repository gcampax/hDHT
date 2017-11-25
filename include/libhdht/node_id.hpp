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
#include <cstdint>
#include <cstring>
#include <string>
#include <cassert>

#include "geo.hpp"

namespace libhdht
{

class NodeIDRange;

class NodeID
{
public:
    friend class NodeIDRange;
    static const size_t size = 20;

private:
    uint8_t m_parts[size];
public:

    NodeID();
    NodeID(const std::string& str);
    NodeID(const GeoPoint2D&, uint8_t resolution);

    GeoPoint2D to_point() const;
    // check that the low 160-mask bits are all 0
    // (meaning that this is a valid node id in a DHT of resolution mask)
    bool has_mask(uint8_t mask) const;

    int bit_at(uint8_t pos) const
    {
        return (m_parts[pos / 8] >> (pos % 8)) & 0x1;
    }
    void set_bit_at(uint8_t pos, int bit)
    {
        m_parts[pos / 8] = (m_parts[pos / 8] & ~(1 << (pos % 8))) | (bit << (pos % 8));
    }

    bool operator==(const NodeID& o) const
    {
        return memcmp(m_parts, o.m_parts, sizeof(m_parts)) == 0;
    }
    bool operator<(const NodeID& o) const
    {
        return memcmp(m_parts, o.m_parts, sizeof(m_parts)) < 0;
    }

    const uint8_t* get_buffer() const
    {
        return m_parts;
    }
    uint8_t* get_buffer()
    {
        return m_parts;
    }

    std::string to_string() const;
};

class NodeIDRange
{
private:
    NodeID m_from;
    uint8_t m_mask;
public:
    NodeIDRange() : m_from(), m_mask(0)
    {}
    NodeIDRange(const NodeID& from, uint8_t mask) : m_from(from), m_mask(mask)
    {
        assert(mask <= 8*NodeID::size);
    }

    const NodeID& from() const
    {
        return m_from;
    }
    NodeID& from()
    {
        return m_from;
    }
    // the number of high bits in m_from that should be considered
    uint8_t mask() const
    {
        return m_mask;
    }
    void increase_mask()
    {
        m_mask++;
    }

    bool contains(const NodeID&) const;
    bool contains(const NodeIDRange&) const;

    bool has_mask(uint8_t mask) const;

    std::string to_string() const;
};

}
