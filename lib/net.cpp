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

#include <arpa/inet.h>

namespace libhdht
{

namespace net
{

std::string
Address::to_string() const
{
    // address + [] + port + null terminator
    char buffer[INET6_ADDRSTRLEN + 2 + 5 + 1];
    if (family() == AF_INET) {
        const sockaddr_in* ipv4_address = (const sockaddr_in*) &m_address;
        inet_ntop(AF_INET, &ipv4_address->sin_addr, buffer, sizeof(buffer));
        int written = strlen(buffer);
        snprintf(buffer+written, sizeof(buffer)-written, ":%d", htons(ipv4_address->sin_port));
    } else {
        const sockaddr_in6* ipv6_address = (const sockaddr_in6*) &m_address;
        buffer[0] = '[';
        inet_ntop(AF_INET6, &ipv6_address->sin6_addr, buffer+1, sizeof(buffer)-1);
        int written = strlen(buffer);
        snprintf(buffer+written, sizeof(buffer)-written, "]:%d", htons(ipv6_address->sin6_port));
    }
    return buffer;
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
