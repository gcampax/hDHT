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
#include <string>
#include <vector>

namespace libhdht {

namespace uv {
class Loop;
class Error;
}

namespace net {

class Error : public std::runtime_error
{
public:
    Error(const char* msg) : std::runtime_error(msg) {}
};

class Address
{
private:
    sockaddr_storage m_address;

public:
    Address()
    {
        memset(&m_address, 0, sizeof(m_address));
    }
    Address(uint16_t port)
    {
        memset(&m_address, 0, sizeof(m_address));
        sockaddr_in6* ipv6_address = (sockaddr_in6*)&m_address;
        ipv6_address->sin6_family = AF_INET6;
        ipv6_address->sin6_port = htons(port);
    }
    Address(socklen_t length, struct sockaddr* addr)
    {
        memcpy(&m_address, addr, length);
    }
    Address(const std::string&);

    bool is_valid() const
    {
        return m_address.ss_family != 0;
    }

    sa_family_t family() const
    {
        return m_address.ss_family;
    }

    uint16_t get_port() const
    {
        if (m_address.ss_family == 0)
            return 0;
        if (m_address.ss_family == AF_INET)
            return ntohs(((sockaddr_in*)&m_address)->sin_port);
        else
            return ntohs(((sockaddr_in6*)&m_address)->sin6_port);
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

class Name
{
private:
    std::string m_hostname;
    uint16_t m_port;

public:
    Name(const std::string& hostname, uint16_t port) : m_hostname(hostname), m_port(port) {}
    Name(const std::string& hostandport);

    void resolve(uv::Loop& loop, std::function<void(uv::Error, const std::vector<net::Address>&)> callback) const;
    template<typename Callback>
    void resolve(uv::Loop& loop, Callback&& callback) const {
        resolve(loop, std::function<void(uv::Error)>(std::forward<Callback>(callback)));
    }

    std::vector<net::Address> resolve_sync() const;
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
