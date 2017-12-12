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
#include <type_traits>

#include "marshal.hpp"

namespace libhdht
{

namespace protocol
{

static const int MASTER_OBJECT_ID = 1;

enum class SetLocationResult : uint8_t
{
    SameServer,
    DifferentServer
};

enum class ClientRegistrationResult : uint8_t
{
    WrongServer,
    ClientCreated,
    ClientAlreadyExists
};

typedef std::tuple<net::Address, NodeIDRange> AddressAndRange;
typedef std::tuple<ClientRegistrationResult, NodeID> ClientRegistrationReply;
typedef std::tuple<SetLocationResult, NodeID, net::Address> SetLocationReply;
typedef std::unordered_map<std::string, std::string> MetadataType;


// Step 1: forward declare all classes

#define begin_class(name) \
    class name##Proxy; \
    class name##Stub;
#define end_class
#define request(return_type, opcode, ...)
#include "protocol.inc.hpp"
#undef request
#undef end_class
#undef begin_class

// Step 2: declare opcodes

#define begin_class(name)
#define end_class
#define request(return_type, opcode, ...) ,opcode
enum class Opcode : uint16_t
{
invalid
#include "protocol.inc.hpp"
,max_opcode
};
#undef request
#undef end_class
#undef begin_class

// Step 3: declare proxies

#define begin_class(name) class name##Proxy : public rpc::Proxy {\
public:\
    typedef name##Proxy proxy_type; \
    typedef name##Stub stub_type; \
    name##Proxy(std::shared_ptr<rpc::Peer> peer, uint64_t object_id) : rpc::Proxy(peer, object_id) {}
#define end_class };
#define request(return_type, opcode, ...) \
    template<typename Callback, typename... Args> void invoke_##opcode(Callback&& callback, Args&&... args) { \
        rpc::impl::proxy_invoker<return_type, __VA_ARGS__> invoker;\
        invoker(get_peer(), (uint16_t)Opcode::opcode, get_object_id(), std::forward<Args>(args)..., std::forward<Callback>(callback));\
    }
#include "protocol.inc.hpp"
#undef request
#undef end_class
#undef begin_class

// Step 4: declare stubs

#define begin_class(name) class name##Stub : public rpc::Stub {\
public:\
    typedef name##Proxy proxy_type; \
    typedef name##Stub stub_type; \
    name##Stub(std::shared_ptr<rpc::Peer> peer, uint64_t object_id) : rpc::Stub(peer, object_id) {}\
protected:\
    virtual void dispatch_request(int16_t opcode, uint64_t request_id, const uv::Buffer& buffer) override;
#define end_class };
#define request(return_type, opcode, ...) \
    virtual void handle_##opcode(uint64_t request_id, __VA_ARGS__) = 0; \
    template<typename... Args> \
    void reply_##opcode(uint64_t request_id, Args&&... args) {\
        std::shared_ptr<rpc::Peer> peer = get_peer();\
        if (!peer) {\
            log(LOG_ERR, "Reply to request " #opcode " dropped (peer was garbage collected)");\
            return;\
        }\
        rpc::impl::reply_invoker<return_type> invoker; \
        invoker(peer, request_id, std::forward<Args>(args)...); \
    }
#include "protocol.inc.hpp"
#undef request
#undef end_class
#undef begin_class

template<typename T>
struct EmptyRequest {
    uv::Buffer marshal() {
        return uv::Buffer(nullptr, 0);
    }
    static T unmarshal(const uv::Buffer&) {
        return T();
    }
};

const char *get_request_name(uint16_t opcode);

}
}

