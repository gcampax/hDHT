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

#include "uv.hpp"
#include "rpc.hpp"

namespace libhdht
{

namespace rpc
{

class BufferWriter
{
private:
    uint8_t* m_storage;
    size_t m_length;
    size_t m_capacity;
    bool m_closed;

public:
    BufferWriter();
    ~BufferWriter();

    uv::Buffer close();
    void reserve(size_t capacity);

    void write(const uint8_t* buffer, size_t length, bool adjust_endian = false);

    template<typename T>
    void write(T t)
    {
        write((const uint8_t*)&t, sizeof(t), true);
    }

    void write(const NodeID& node_id)
    {
        write(node_id.get_buffer(), NodeID::size, false);
    }

    void write(const NodeIDRange& range)
    {
        write(range.from());
        write(range.log_size());
    }

    void write(const net::Address& address)
    {
        write(address.to_string());
    }

    void write(const std::string& str)
    {
        if (str.size() > std::numeric_limits<uint16_t>::max())
            throw std::length_error("Strings larger than 65,536 characters cannot be marshalled");
        write(static_cast<uint16_t>(str.size()));
        write((const uint8_t*)str.c_str(), str.size(), false);
    }
};

struct ReadError : std::runtime_error
{
    ReadError(const char *msg) : std::runtime_error(msg) {}
    ReadError(const std::string& msg) : std::runtime_error(msg) {}
};

class BufferReader
{
private:
    const uv::Buffer& m_buffer;
    size_t m_off;

    void read(uint8_t* into, size_t length, bool adjust_endian = false);

public:
    BufferReader(const uv::Buffer& buffer) : m_buffer(buffer), m_off(0) {}

    template<typename T>
    T read()
    {
        T t;
        read((uint8_t*) &t, sizeof(t), true);
        return t;
    }
};

template<>
inline NodeID
BufferReader::read<NodeID>()
{
    NodeID id;
    read(id.get_buffer(), NodeID::size, false);
    return id;
}

template<>
inline NodeIDRange
BufferReader::read<NodeIDRange>()
{
    NodeID from = read<NodeID>();
    uint8_t log_size = read<uint8_t>();
    if (log_size > 8*NodeID::size)
        throw ReadError("Invalid NodeID range size");
    return NodeIDRange(std::move(from), log_size);
}

template<>
inline std::string
BufferReader::read<std::string>()
{
    size_t length = read<uint16_t>();
    std::string str;
    str.resize(length);
    read((uint8_t*)str.data(), length, false);
    return str;
}

template<>
inline net::Address
BufferReader::read<net::Address>()
{
    return net::Address(read<std::string>());
}

namespace impl
{

template<typename... Args>
struct pack_marshaller {
};

template<typename First, typename... Rest>
struct pack_marshaller<First, Rest...> {
    static void to_buffer(BufferWriter& writer, const First& first, const Rest&... rest)
    {
        pack_marshaller<First>::to_buffer(writer, first);
        pack_marshaller<Rest...>::to_buffer(writer, rest...);
    }

    static std::tuple<First, Rest...> from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        return std::tuple<First, Rest...>(pack_marshaller<First>::from_buffer(peer, reader), pack_marshaller<Rest>::from_buffer(peer, reader)...);
    }
};

template<>
struct pack_marshaller<> {
    static void to_buffer(BufferWriter& writer)
    {
        // nothing to do
    }

    static void from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        // nothing to do
    }
};

template<>
struct pack_marshaller<void> {
    static void to_buffer(BufferWriter& writer)
    {
        // nothing to do
    }

    static void from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        // nothing to do
    }
};

template<typename Type, typename Enable = void>
struct single_marshaller
{};

template<typename Type>
struct single_marshaller<Type, typename std::enable_if<std::is_arithmetic<Type>::value>::type>
{
    static void to_buffer(BufferWriter& writer, Type obj)
    {
        writer.write(obj);
    }

    static Type from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        return reader.read<Type>();
    }
};

template<>
struct single_marshaller<NodeID>
{
    static void to_buffer(BufferWriter& writer, const NodeID& obj)
    {
        writer.write(obj);
    }

    static NodeID from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        return reader.read<NodeID>();
    }
};

template<>
struct single_marshaller<net::Address>
{
    static void to_buffer(BufferWriter& writer, const net::Address& obj)
    {
        writer.write(obj);
    }

    static net::Address from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        return reader.read<net::Address>();
    }
};

template<>
struct single_marshaller<NodeIDRange>
{
    static void to_buffer(BufferWriter& writer, const NodeIDRange& obj)
    {
        writer.write(obj);
    }

    static NodeIDRange from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        return reader.read<NodeIDRange>();
    }
};

template<typename... Args>
struct single_marshaller<std::tuple<Args...>>
{
private:
    template<size_t... I>
    static void helper(BufferWriter& writer, const std::tuple<Args...>& obj, std::index_sequence<I...>)
    {
        pack_marshaller<Args...>::to_buffer(writer, std::get<I>(obj)...);
    }

public:
    static void to_buffer(BufferWriter& writer, const std::tuple<Args...>& obj)
    {
        helper(writer, obj, std::index_sequence_for<Args...>());
    }

    static std::tuple<Args...> from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        return std::tuple<Args...>(single_marshaller<Args>::from_buffer(peer, reader)...);
    }
};

// Note: for objects, the "IDL" always uses the Proxy type, but for outgoing requests we expect
// the stub type
//
// This means that you cannot send a proxy back on the wire.
// This should be ok by our means.
// It also means we don't check that the proxy is correct for the given peer. Be careful.
//
template<typename Type>
struct single_marshaller<std::shared_ptr<Type>, typename std::enable_if<std::is_base_of<rpc::Proxy, Type>::value>::type>
{
    static std::shared_ptr<Type> from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        uint64_t object_id = reader.read<uint64_t>();
        if (object_id == 0)
            return nullptr;
        return peer.get_proxy<Type>(object_id);
    }
};

template<typename Type>
struct single_marshaller<std::shared_ptr<Type>, typename std::enable_if<std::is_base_of<rpc::Stub, Type>::value>::type>
{
    static void to_buffer(BufferWriter& writer, const std::shared_ptr<Type>& obj)
    {
        writer.write(obj ? obj->get_object_id() : 0);
    }
};

template<typename Type>
struct pack_marshaller<Type> {
    static void to_buffer(BufferWriter& writer, const Type& type)
    {
        single_marshaller<Type>::to_buffer(writer, type);
    }

    static Type from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        return single_marshaller<Type>::from_buffer(peer, reader);
    }
};

template<typename Arg, typename Enable = void>
struct convert_proxy_to_stub
{
    typedef Arg type;
};

template<typename Arg>
struct convert_proxy_to_stub<std::shared_ptr<Arg>, typename std::enable_if<std::is_base_of<rpc::Proxy, Arg>::value>::type>
{
    typedef std::shared_ptr<typename Arg::stub_type> type;
};

template<typename Return, typename... Args>
struct proxy_invoker {
    void operator()(std::shared_ptr<rpc::Peer> peer, uint16_t opcode, uint64_t object_id, const typename convert_proxy_to_stub<Args>::type&... args, const std::function<void(rpc::Error*, Return)>& callback) const {
        BufferWriter writer;
        pack_marshaller<typename convert_proxy_to_stub<Args>::type...>::to_buffer(writer, args...);
        peer->invoke_request(opcode, object_id, writer.close(), [peer, callback](rpc::Error* error, const uv::Buffer* buffer) {
            if (error) {
                callback(error, Return());
            } else {
                BufferReader reader(*buffer);
                callback(nullptr, single_marshaller<Return>::from_buffer(*peer, reader));
            }
        });
    }
};

// specialization for calls that return void
template<typename... Args>
struct proxy_invoker<void, Args...>  {
    void operator()(std::shared_ptr<rpc::Peer> peer, uint16_t opcode, uint64_t object_id, const typename convert_proxy_to_stub<Args>::type&... args, const std::function<void(rpc::Error*)>& callback) const {
        BufferWriter writer;
        pack_marshaller<typename convert_proxy_to_stub<Args>::type...>::to_buffer(writer, args...);
        peer->invoke_request(opcode, object_id, writer.close(), [peer, callback](rpc::Error* error, const uv::Buffer* buffer) {
            if (error) {
                callback(error);
            } else {
                callback(nullptr);
            }
        });
    }
};

template<typename Return>
struct reply_invoker {
    void operator()(std::shared_ptr<rpc::Peer> peer, uint16_t opcode, uint64_t request_id, const Return& args) const {
        BufferWriter writer;
        single_marshaller<Return>::to_buffer(writer, args);
        peer->send_reply(opcode, request_id, writer.close());
    }
};

// specialization for calls that return void
template<>
struct reply_invoker<void> {
    void operator()(std::shared_ptr<rpc::Peer> peer, uint16_t opcode, uint64_t request_id) const {
        uv::Buffer buffer;
        peer->send_reply(opcode, request_id, std::move(buffer));
    }
};

// specialization for calls that return a tuple
template<typename... Args>
struct reply_invoker<std::tuple<Args...>> {
    void operator()(std::shared_ptr<rpc::Peer> peer, uint16_t opcode, uint64_t request_id, const Args&... args) const {
        BufferWriter writer;
        pack_marshaller<Args...>::to_buffer(writer, args...);
        peer->send_reply(opcode, request_id, writer.close());
    }
};

}

}

}
