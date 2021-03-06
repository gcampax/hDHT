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

#include <libhdht/uv.hpp>
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
        write(range.mask());
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
    uint8_t mask = read<uint8_t>();
    if (mask > 8*NodeID::size)
        throw ReadError("Invalid NodeID range size");
    return NodeIDRange(std::move(from), mask);
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

namespace impl
{

template<typename... Args>
struct pack_marshaller {
};

template<typename Type, typename Enable = void>
struct single_marshaller
{};

template<typename First, typename... Rest>
struct pack_marshaller<First, Rest...> {
    static void to_buffer(BufferWriter& writer, const First& first, const Rest&... rest)
    {
        single_marshaller<First>::to_buffer(writer, first);
        pack_marshaller<Rest...>::to_buffer(writer, rest...);
    }

    static std::tuple<First, Rest...> from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        First first = single_marshaller<First>::from_buffer(peer, reader);
        std::tuple<Rest...> rest = pack_marshaller<Rest...>::from_buffer(peer, reader);
        return std::tuple_cat(std::make_tuple(std::move(first)), std::move(rest));
    }
};

template<>
struct pack_marshaller<> {
    static void to_buffer(BufferWriter& writer)
    {
        // nothing to do
    }

    static std::tuple<> from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        // nothing to do
        return std::tuple<>();
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

template<typename Type>
struct single_marshaller<Type,
    typename std::enable_if<std::is_arithmetic<Type>::value || std::is_enum<Type>::value>::type>
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

#define MAKE_SIMPLE_MARSHALLER(type) \
    template<>\
    struct single_marshaller<type>\
    {\
        static void to_buffer(BufferWriter& writer, const type& obj)\
        {\
            writer.write(obj);\
        }\
        static type from_buffer(rpc::Peer& peer, BufferReader& reader)\
        {\
            return reader.read<type>();\
        }\
    };\

MAKE_SIMPLE_MARSHALLER (NodeID)
MAKE_SIMPLE_MARSHALLER (NodeIDRange)
MAKE_SIMPLE_MARSHALLER (std::string)

template<>
struct single_marshaller<net::Address>
{
    static void to_buffer(BufferWriter& writer, const net::Address& obj)
    {
        writer.write(obj.to_string());
    }

    static net::Address from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        return net::Address(reader.read<std::string>());
    }
};

template<>
struct single_marshaller<GeoPoint2D>
{
    static void to_buffer(BufferWriter& writer, const GeoPoint2D& obj)
    {
        writer.write(obj.latitude);
        writer.write(obj.longitude);
    }

    static GeoPoint2D from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        return GeoPoint2D{ reader.read<double>(), reader.read<double>() };
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
        return pack_marshaller<Args...>::from_buffer(peer, reader);
    }
};

template<typename First, typename Second>
struct single_marshaller<std::pair<First, Second>>
{
    static void to_buffer(BufferWriter& writer, const std::pair<First, Second>& obj)
    {
        single_marshaller<First>::to_buffer(writer, obj.first);
        single_marshaller<Second>::to_buffer(writer, obj.second);
    }

    static std::pair<First, Second> from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        First first = single_marshaller<First>::from_buffer(peer, reader);
        Second second = single_marshaller<Second>::from_buffer(peer, reader);
        return std::make_pair(std::move(first), std::move(second));
    }
};

template<typename Key, typename Value>
struct single_marshaller<std::unordered_map<Key, Value>>
{
    static void to_buffer(BufferWriter& writer, const std::unordered_map<Key, Value>& obj)
    {
        if (obj.size() > std::numeric_limits<uint16_t>::max())
            throw std::length_error("Maps with more than 65536 elements cannot be marshalled");
        writer.write(static_cast<uint16_t>(obj.size()));
        for (const auto& iter : obj)
            single_marshaller<std::pair<Key, Value>>::to_buffer(writer, iter);
    }

    static std::unordered_map<Key, Value> from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        size_t n = reader.read<uint16_t>();
        std::unordered_map<Key, Value> map;
        for (size_t i = 0; i < n; i++)
            map.insert(single_marshaller<std::pair<Key, Value>>::from_buffer(peer, reader));
        return map;
    }
};

template<typename Type>
struct single_marshaller<std::vector<Type>>
{
    static void to_buffer(BufferWriter& writer, const std::vector<Type>& obj)
    {
        if (obj.size() > std::numeric_limits<uint16_t>::max())
            throw std::length_error("Vectors with more than 65536 elements cannot be marshalled");
        writer.write(static_cast<uint16_t>(obj.size()));
        for (const auto& iter : obj)
            single_marshaller<Type>::to_buffer(writer, iter);
    }

    static std::vector<Type> from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        size_t n = reader.read<uint16_t>();
        std::vector<Type> result;
        result.reserve(n);
        for (size_t i = 0; i < n; i++)
            result.push_back(single_marshaller<Type>::from_buffer(peer, reader));
        return result;
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

// specialization for calls that return a tuple
template<typename... Args, typename... ReturnArgs>
struct proxy_invoker<std::tuple<ReturnArgs...>, Args...>  {
private:
    template<typename Callback, class Tuple, std::size_t... I>
    static void helper(Callback&& callback, Tuple&& tuple, std::index_sequence<I...>)
    {
        callback(nullptr, std::get<I>(std::forward<Tuple>(tuple))...);
    }

public:
    void operator()(std::shared_ptr<rpc::Peer> peer, uint16_t opcode, uint64_t object_id, const typename convert_proxy_to_stub<Args>::type&... args, const std::function<void(rpc::Error*, ReturnArgs...)>& callback) const {
        BufferWriter writer;
        pack_marshaller<typename convert_proxy_to_stub<Args>::type...>::to_buffer(writer, args...);
        peer->invoke_request(opcode, object_id, writer.close(), [peer, callback](rpc::Error* error, const uv::Buffer* buffer) {
            if (error) {
                callback(error, ReturnArgs()...);
            } else {
                BufferReader reader(*buffer);
                auto tuple = pack_marshaller<ReturnArgs...>::from_buffer(*peer, reader);
                helper(std::move(callback), std::move(tuple), std::index_sequence_for<ReturnArgs...>());
            }
        });
    }
};

template<typename Return>
struct reply_invoker {
    void operator()(std::shared_ptr<rpc::Peer> peer, uint64_t request_id, const Return& args) const {
        BufferWriter writer;
        single_marshaller<Return>::to_buffer(writer, args);
        peer->send_reply(request_id, writer.close());
    }
};

// specialization for calls that return void
template<>
struct reply_invoker<void> {
    void operator()(std::shared_ptr<rpc::Peer> peer, uint64_t request_id) const {
        uv::Buffer buffer;
        peer->send_reply(request_id, std::move(buffer));
    }
};

// specialization for calls that return a tuple
template<typename... Args>
struct reply_invoker<std::tuple<Args...>> {
    void operator()(std::shared_ptr<rpc::Peer> peer, uint64_t request_id, const Args&... args) const {
        BufferWriter writer;
        pack_marshaller<Args...>::to_buffer(writer, args...);
        peer->send_reply(request_id, writer.close());
    }
};

}

}

}
