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
#include <uv.h>

#include "net.hpp"

// wrappers and helpers for libuv's event loop

namespace libhdht
{

namespace uv
{

class Error : public std::exception
{
private:
    int m_status;

public:
    Error(int status) : m_status(status) {}

    virtual const char* what() const noexcept override
    {
        return uv_strerror(m_status);
    }

    explicit operator bool() const
    {
        return m_status != 0;
    }
    bool operator==(int status) const
    {
        return m_status == status;
    }

    static void check(int status)
    {
        if (status != 0)
            throw uv::Error(status);
    }
};

class TCPSocket;

class Loop
{
private:
    uv_loop_t m_loop;

public:
    Loop() {
        uv_loop_init(&m_loop);
    }
    ~Loop() {
        uv_loop_close(&m_loop);
    }

    uv_loop_t *loop() {
        return &m_loop;
    }

    void run() {
        uv_run(&m_loop, UV_RUN_DEFAULT);
    }
    void stop() {
        uv_stop(&m_loop);
    }
};

class Buffer : public uv_buf_t
{
private:
    bool owns_memory;

public:
    Buffer(const uint8_t* buffer, size_t length, bool own_memory = false) {
        base = (char*)buffer;
        len = length;
        owns_memory = own_memory;
    }
    Buffer(const Buffer& buffer) = delete;
    Buffer(Buffer&& other) {
        base = other.base;
        len = other.len;
        owns_memory = other.owns_memory;
        other.owns_memory = false;
    }
    ~Buffer() {
        if (owns_memory)
            free(base);
    }
    // Buffer has some tricky properties when it comes to
    // copy and move semantics, and to make sure the code
    // using Buffer knows what's going on, make sure it
    // always goes through constructors and not assignments
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&&) = delete;

    explicit operator bool() const {
        return base != nullptr;
    }

    static Buffer allocate(size_t size) {
        Buffer buf(nullptr, 0);
        buf.base = (char*)malloc(size);
        if (!buf)
            return buf;
        buf.len = size;
        return buf;
    }

    Buffer clone() const {
        Buffer new_buf(nullptr, 0);
        new_buf.base = (char*)malloc(len);
        if (!new_buf) {
            new_buf.len = 0;
            return new_buf;
        }
        new_buf.owns_memory = true;
        memcpy(new_buf.base, base, len);
        new_buf.len = len;
        return new_buf;
    }
};

class TCPSocket : private uv_tcp_t
{
private:
    template<typename T>
    static inline T* handle_cast(TCPSocket *socket) {
        return (T*)(static_cast<uv_tcp_t*>(socket));
    }
    template<typename T>
    static inline TCPSocket* handle_downcast(T *socket) {
        return static_cast<TCPSocket*>((uv_tcp_t*)(socket));
    }

    bool m_usable = false;

public:
    TCPSocket(uv_loop_t* loop);
    TCPSocket(Loop& loop) : TCPSocket(loop.loop()) {}
    TCPSocket(const TCPSocket&) = delete;
    TCPSocket& operator=(const TCPSocket&) = delete;
    TCPSocket& operator=(TCPSocket&&) = delete;
    TCPSocket(TCPSocket&& other) = delete;

    virtual ~TCPSocket();

    bool is_usable() const
    {
        return m_usable;
    }
    net::Address get_peer_name() const;

    void connect(const net::Address& address);
    void listen(const net::Address& address);
    void ref()
    {
        uv_ref(handle_cast<uv_handle_t>(this));
    }
    void unref()
    {
        uv_unref(handle_cast<uv_handle_t>(this));
    }
    void close();

    void start_reading();
    void stop_reading() {
        uv_read_stop(handle_cast<uv_stream_t>(this));
    }
    void write(uint64_t req_id, const Buffer* buffers, size_t nbuffers);
    void accept(TCPSocket *client);

    // override in subclasses to handle async IO results
    virtual void connected(Error) {}
    virtual void closed() {
        // free any memory associated with this socket
        delete this;
    }
    virtual void listen_error(Error err)
    {
        log(LOG_ERR, "Failed to setup listening socket: %s", err.what());
        close();
    }
    virtual void new_connection() {}
    virtual void read_callback(Error err, const uv::Buffer&)
    {
        if (err)
            close();
    }
    virtual void write_complete(uint64_t, Error err)
    {
        if (err)
            close();
    }
};

}
}
