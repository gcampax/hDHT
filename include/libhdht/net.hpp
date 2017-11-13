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

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <cstring>
#include <functional>

namespace libhdht {

namespace net {

class Address
{
private:
    sockaddr_storage m_address;

public:
    Address()
    {
        std::memset(&m_address, 0, sizeof(m_address));
    }
    Address(const std::string&);

    sa_family_t family() const
    {
        return m_address.ss_family;
    }

    const sockaddr *get() const
    {
        return (const sockaddr*)&m_address;
    }
    sockaddr *get()
    {
        return (sockaddr*)&m_address;
    }
    size_t size() const
    {
        if (m_address.ss_family == 0)
            return 0;
        if (m_address.ss_family == AF_INET)
            return sizeof(sockaddr_in);
        else
            return sizeof(sockaddr_in6);
    }

    bool operator==(const net::Address& other) const
    {
        if (family() != other.family())
            return false;
        return memcmp(&m_address, &other.m_address, size()) == 0;
    }

    std::string to_string() const;
};

}

}

namespace std {
    template<>
    struct hash<libhdht::net::Address>
    {
        size_t operator()(const libhdht::net::Address& address) const;
    };
}
