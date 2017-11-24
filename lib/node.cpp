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


}
