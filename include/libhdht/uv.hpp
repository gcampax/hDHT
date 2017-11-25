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
#include <streambuf>
#include <uv.h>

#include "net.hpp"

// wrappers and helpers for libuv's event loop

namespace libhdht
{

namespace uv
{

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
    Buffer(const uint8_t* buffer, size_t length, bool own_memory = false)
    {
        base = (char*)buffer;
        len = length;
        owns_memory = own_memory;
    }
    Buffer(const Buffer& buffer) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other)
    {
        base = other.base;
        len = other.len;
        owns_memory = other.owns_memory;
        other.owns_memory = false;
    }
    Buffer& operator=(Buffer&& other)
    {
        if (owns_memory)
            free(base);
        base = other.base;
        len = other.len;
        owns_memory = other.owns_memory;
        other.owns_memory = false;
        return *this;
    }
    Buffer()
    {
        base = nullptr;
        len = 0;
        owns_memory = false;
    }
    ~Buffer()
    {
        if (owns_memory)
            free(base);
    }

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

class BufferInputStringBuf : public std::streambuf
{
private:
    uv::Buffer m_buffer;

public:
    BufferInputStringBuf(uv::Buffer&& buffer) : m_buffer(std::move(buffer))
    {
        setg(m_buffer.base, m_buffer.base, m_buffer.base+m_buffer.len);
    }
    virtual ~BufferInputStringBuf() {}

    virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override
    {
        switch (dir) {
        case std::ios_base::beg:
            return seekpos(off, which);
        case std::ios_base::end:
            return seekpos(m_buffer.len - off, which);
            break;
        case std::ios_base::cur:
            return seekpos((gptr() - eback()) + off, which);
        default:
            return -1;
        }
    }
    virtual pos_type seekpos(pos_type pos, std::ios_base::openmode which) override
    {
        size_t off = std::min(std::max(pos_type(0), pos), pos_type(m_buffer.len));
        setg(m_buffer.base, m_buffer.base + off, m_buffer.base+m_buffer.len);
        return off;
    }

    virtual std::streamsize showmanyc() override
    {
        // underflow will always return -1
        return -1;
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

    bool m_usable = true;

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
    virtual void read_callback(Error err, uv::Buffer&&)
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

class TTY : public uv_tty_t
{
private:
    template<typename T>
    static inline T* handle_cast(TTY *socket) {
        return (T*)(static_cast<uv_tty_t*>(socket));
    }
    template<typename T>
    static inline TTY* handle_downcast(T *socket) {
        return static_cast<TTY*>((uv_tty_t*)(socket));
    }

public:
    TTY(uv_loop_t* loop, int fd);
    TTY(uv::Loop& loop, int fd) : TTY(loop.loop(), fd) {}
    virtual ~TTY();

    virtual void read_line(Error err, uv::Buffer&&)
    {
        if (err)
            close();
    }

    void ref()
    {
        uv_ref(handle_cast<uv_handle_t>(this));
    }
    void unref()
    {
        uv_unref(handle_cast<uv_handle_t>(this));
    }
    void close();
    virtual void closed()
    {
        // free any memory associated with this socket
        delete this;
    }

    void start_reading();
    void stop_reading()
    {
        uv_read_stop(handle_cast<uv_stream_t>(this));
    }
};

}
}
