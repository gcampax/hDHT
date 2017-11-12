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

class TCPSocket;

class Loop
{
friend class TCPSocket;

private:
    uv_loop_t m_loop;

    uv_loop_t *loop() {
        return &m_loop;
    }

public:
    Loop() {
        uv_loop_init(&m_loop);
    }
    ~Loop() {
        uv_loop_close(&m_loop);
    }

    void run() {
        uv_run(&m_loop, UV_RUN_DEFAULT);
    }
    void stop() {
        uv_stop(&m_loop);
    }
};

class TCPSocket : private uv_tcp_t, public std::enable_shared_from_this<TCPSocket>
{
private:
    TCPSocket(uv_loop_t* loop);

    template<typename T>
    static inline T* handle_cast(TCPSocket *socket) {
        return (T*)(static_cast<uv_tcp_t*>(socket));
    }
    template<typename T>
    static inline TCPSocket* handle_downcast(T *socket) {
        return static_cast<TCPSocket*>((uv_tcp_t*)(socket));
    }

public:
    ~TCPSocket() {
        uv_close(handle_cast<uv_handle_t>(this), nullptr);
    }

    static std::shared_ptr<TCPSocket> create(Loop& loop) {
        return std::shared_ptr<TCPSocket>(new TCPSocket(loop.loop()));
    }

    void connect(const net::Address& address);
    void listen(const net::Address& address);

    void start_reading();
    void stop_reading() {
        uv_read_stop(handle_cast<uv_stream_t>(this));
    }
    void write(uint64_t req_id, const uint8_t* buffer, size_t length);

    // override in subclasses to handle async IO results
    virtual void connected(int status) {}
    virtual void listen_error(int status) {}
    virtual void new_connection(std::shared_ptr<TCPSocket> socket) {}
    virtual void read_callback(ssize_t nread, const uint8_t* buffer, size_t length) {}
    virtual void write_complete(uint64_t req_id, int status) {}
};

}
}
