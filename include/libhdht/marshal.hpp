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

namespace libhdht
{

class BufferWriter
{
private:
    uint8_t* m_storage;
    size_t m_length;
    size_t m_capacity;
    bool m_closed = false;

public:
    BufferWriter();
    ~BufferWriter();

    uv::Buffer close();

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
};

class BufferReader
{
private:
    const uv::Buffer& m_buffer;
    size_t m_off;

public:
    BufferReader(const uv::Buffer& buffer) : m_buffer(buffer), m_off(0) {}

    template<typename T>
    T read()
    {
        // do something
        return T();
    }
};

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
    static void to_buffer(BufferWriter& writer, const std::shared_ptr<typename Type::stub_type>& obj)
    {
        writer.write(obj ? obj->get_object_id() : 0);
    }

    static std::shared_ptr<Type> from_buffer(rpc::Peer& peer, BufferReader& reader)
    {
        uint64_t object_id = reader.read<uint64_t>();
        if (object_id == 0)
            return nullptr;
        return peer.get_proxy<Type>(object_id);
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

}
