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

#include <arpa/inet.h>
#include <netdb.h>

namespace libhdht
{

namespace net
{

Address::Address(const std::string& str)
{
    if (str.empty()) {
        memset(&m_address, 0, sizeof(m_address));
        return;
    }

    if (str[0] == '[') {
        // ipv6
        size_t close_bracket = str.find(']');
        if (close_bracket == std::string::npos)
            throw net::Error("Invalid IPv6 address (missing close bracket)");

        sockaddr_in6 in6_addr;
        in6_addr.sin6_family = AF_INET6;
        if (!inet_pton(AF_INET6, str.substr(1, close_bracket-1).c_str(), &in6_addr.sin6_addr))
            throw net::Error("Invalid IPv6 address");

        if (close_bracket == str.size()-1) {
            in6_addr.sin6_port = htons(protocol::DEFAULT_PORT);
        } else {
            if (close_bracket >= str.size()-2 || str[close_bracket+1] != ':')
                throw net::Error("Junk at end of IPv6 address");
            in6_addr.sin6_port = htons(atoi(&str.c_str()[close_bracket+2]));
        }
        memcpy(&m_address, &in6_addr, sizeof(in6_addr));
    } else {
        size_t colon = str.find(':');
        sockaddr_in in4_addr;
        in4_addr.sin_family = AF_INET;
        if (!inet_pton(AF_INET, str.substr(0, colon).c_str(), &in4_addr.sin_addr))
            throw net::Error("Invalid IPv4 address");

        if (colon == std::string::npos) {
            in4_addr.sin_port = htons(protocol::DEFAULT_PORT);
        } else {
            if (colon >= str.size()-1)
                throw net::Error("Junk at end of IPv4 address");
            in4_addr.sin_port = htons(atoi(&str.c_str()[colon+1]));
        }
        memcpy(&m_address, &in4_addr, sizeof(in4_addr));
    }
}

std::string
Address::to_string() const
{
    if (!is_valid())
        return std::string();

    // address + [] + port + null terminator
    char buffer[INET6_ADDRSTRLEN + 2 + 5 + 1];
    if (family() == AF_INET) {
        const sockaddr_in* ipv4_address = (const sockaddr_in*) &m_address;
        inet_ntop(AF_INET, &ipv4_address->sin_addr, buffer, sizeof(buffer));
        int written = strlen(buffer);
        snprintf(buffer+written, sizeof(buffer)-written, ":%d", ntohs(ipv4_address->sin_port));
    } else {
        const sockaddr_in6* ipv6_address = (const sockaddr_in6*) &m_address;
        buffer[0] = '[';
        inet_ntop(AF_INET6, &ipv6_address->sin6_addr, buffer+1, sizeof(buffer)-1);
        int written = strlen(buffer);
        snprintf(buffer+written, sizeof(buffer)-written, "]:%d", ntohs(ipv6_address->sin6_port));
    }
    return buffer;
}

Name::Name(const std::string& name)
{
    size_t colon = name.find(':');

    m_hostname = name.substr(0, colon);
    if (colon < name.size()-1)
        m_port = atoi(&name.c_str()[colon+1]);
    else
        m_port = protocol::DEFAULT_PORT;
}

void
Name::resolve(uv::Loop& loop, std::function<void(uv::Error, const std::vector<net::Address>&)> callback) const
{
    struct request : uv_getaddrinfo_t
    {
        std::function<void(uv::Error, const std::vector<net::Address>&)> callback;
    };
    request* req = new request;
    req->callback = callback;

    struct addrinfo hints = {
        AI_NUMERICHOST,
        AF_UNSPEC,
        SOCK_STREAM,
    };
    char port[7];
    snprintf(port, sizeof(port), "%d", m_port);

    uv_getaddrinfo(loop.loop(), req, [](uv_getaddrinfo_t* uv_req, int status, struct addrinfo* res) {
        uv::Error err(status);
        request* req = static_cast<request*>(uv_req);
        std::vector<net::Address> result;

        if (err) {
            req->callback(err, result);
            uv_freeaddrinfo(res);
            delete req;
            return;
        }

        for (struct addrinfo* iter = res; iter; iter = iter->ai_next)
            result.emplace_back(iter->ai_addrlen, iter->ai_addr);
        req->callback(err, result);
        uv_freeaddrinfo(res);
        delete req;
    }, m_hostname.c_str(), port, &hints);
}

std::vector<net::Address>
Name::resolve_sync() const
{
    struct addrinfo hints = {
        AI_NUMERICHOST,
        AF_UNSPEC,
        SOCK_STREAM,
    };
    char port[7];
    snprintf(port, sizeof(port), "%d", m_port);

    struct addrinfo* res;
    int err = getaddrinfo(m_hostname.c_str(), port, &hints, &res);

    if (err)
        throw std::runtime_error(gai_strerror(err));

    std::vector<net::Address> result;
    for (struct addrinfo* iter = res; iter; iter = iter->ai_next)
        result.emplace_back(iter->ai_addrlen, iter->ai_addr);

    freeaddrinfo(res);
    return result;
}

}

}

namespace std {

size_t
hash<libhdht::net::Address>::operator()(const libhdht::net::Address& address) const
{
    size_t result = 0;
    const size_t *buffer = (const size_t*) address.get();
    for (size_t i = 0; i < address.size()/sizeof(size_t); i++)
        result ^= buffer[i];
    return result;
}

}
