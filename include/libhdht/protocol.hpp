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

#include "marshal.hpp"

namespace libhdht
{

namespace protocol
{

static const int DEFAULT_PORT = 7777;
static const int MASTER_OBJECT_ID = 1;

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

namespace impl {

template<Opcode opcode, typename Return, typename... Args>
struct proxy_invoker {
    void operator()(rpc::Peer *peer, Args&&... args, const std::function<void(uv::Error, Return)>& callback) const {
        // do something
    }

    template<typename Callback>
    void operator()(rpc::Peer *peer, Args&&... args, Callback callback) const {
        (*this)(peer, std::forward<Args>(args)..., std::function<void(uv::Error, Return)>(std::forward<Callback>(callback)));
    }
};

// specialization for calls that return void
template<Opcode opcode, typename... Args>
struct proxy_invoker<opcode, void, Args...>  {
    void operator()(rpc::Peer *peer, Args&&... args, const std::function<void(uv::Error)>& callback) const {
        // do something
    }

    template<typename Callback>
    void operator()(rpc::Peer *peer, Args&&... args, Callback callback) const {
        (*this)(peer, std::forward<Args>(args)..., std::function<void(uv::Error)>(std::forward<Callback>(callback)));
    }
};

template<Opcode opcode, typename Return>
struct reply_invoker {
    void operator()(std::weak_ptr<rpc::Peer> peer, uint64_t request_id, Return&& args) const {
        // do something
    }
};

// specialization for calls that return void
template<Opcode opcode>
struct reply_invoker<opcode, void> {
    void operator()(std::weak_ptr<rpc::Peer> peer, uint64_t request_id) const {
        // do something
    }
};

// specialization for calls that return a tuple
template<Opcode opcode, typename... Args>
struct reply_invoker<opcode, std::tuple<Args...>> {
    void operator()(std::weak_ptr<rpc::Peer> peer, uint64_t request_id, Args&&... args) const {
        // do something
    }
};

}

// Step 3: declare proxies

#define begin_class(name) class name##Proxy : public rpc::Proxy {\
public:\
    typedef name##Proxy proxy_type; \
    typedef name##Stub stub_type; \
    name##Proxy(std::shared_ptr<rpc::Peer> peer, uint64_t object_id) : rpc::Proxy(peer, object_id) {}
#define end_class };
#define request(return_type, opcode, ...) \
    template<typename... Args, typename Callback> void invoke_##opcode(Args&&... args, Callback callback) { \
        impl::proxy_invoker<Opcode::opcode, return_type, __VA_ARGS__> invoker;\
        invoker(get_peer(), std::forward<Args>(args)..., std::forward<Callback>(callback));\
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
        impl::reply_invoker<Opcode::opcode, return_type> invoker; \
        invoker(get_peer(), request_id, std::forward<Args>(args)...); \
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

size_t get_request_size(Opcode opcode);
size_t get_reply_size(Opcode opcode);

}
}

