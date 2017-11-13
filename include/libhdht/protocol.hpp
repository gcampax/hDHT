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

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "uv.h"

namespace libhdht
{

namespace protocol
{

enum class Opcode : uint16_t
{
    Hello
};

struct MessageHeader
{
    uint16_t opcode;
    uint64_t request_id;
} __attribute__((packed));

struct BaseRequest
{
    MessageHeader header;
    uint64_t object_id;
} __attribute__((packed));

struct BaseResponse {
    MessageHeader header;
    uint32_t error;
};

template<typename T>
struct EmptyRequest {
    uv::Buffer marshal() {
        return uv::Buffer(nullptr, 0);
    }
    static T unmarshal(const uv::Buffer&) {
        return T();
    }
};

struct Hello {
    static const Opcode opcode = Opcode::Hello;
    typedef struct HelloRequest : public EmptyRequest<HelloRequest> {} request_type;
    typedef struct HelloResponse : public EmptyRequest<HelloResponse> {} response_type;
};

}
}

