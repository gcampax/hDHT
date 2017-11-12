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

#include <new>

namespace libhdht
{

namespace uv
{

TCPSocket::TCPSocket(uv_loop_t* loop)
{
    uv_tcp_init(loop, this);
}

void
TCPSocket::connect(const net::Address& address)
{
    struct request : uv_connect_t
    {
        std::shared_ptr<TCPSocket> socket;
    };
    auto req = new request;
    req->socket = shared_from_this();

    uv_tcp_connect(req, this, (sockaddr*)(address.get()), [](uv_connect_t *uv_req, int status) {
        request *req = static_cast<request*>(uv_req);
        req->socket->connected(status);
        delete req;
    });
}

void
TCPSocket::listen(const net::Address& address)
{
    uv_listen(handle_cast<uv_stream_t>(this), 0, [](uv_stream_t* server, int status) {
        TCPSocket *self = handle_downcast(server);
        if (status >= 0) {
            uv_loop_t *loop = handle_cast<uv_handle_t>(self)->loop;
            std::shared_ptr<TCPSocket> client_socket(new TCPSocket(loop));
            uv_accept(server, handle_cast<uv_stream_t>(client_socket.get()));
            self->new_connection(client_socket);
        } else {
            self->listen_error(status);
        }
    });
}

static void
alloc_memory(uv_handle_t*, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = new (std::nothrow) char[suggested_size];
    buf->len = buf->base ? suggested_size : 0;
}

void
TCPSocket::start_reading()
{
    uv_read_start(handle_cast<uv_stream_t>(this), alloc_memory, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buffer) {
        handle_downcast(stream)->read_callback(nread, (const uint8_t*)buffer->base, buffer->len);
    });
}

void
TCPSocket::write(uint64_t req_id, const uint8_t* buffer, size_t length)
{
    struct request : uv_write_t
    {
        std::shared_ptr<TCPSocket> socket;
        uv_buf_t buf;
        uint64_t req_id;
    };
    auto req = new (std::nothrow) request();
    if (req == nullptr) {
        write_complete(req_id, UV_ENOBUFS);
        return;
    }

    req->socket = shared_from_this();
    req->req_id = req_id;
    req->buf.base = (char*)malloc(length);
    if (req->buf.base == nullptr) {
        write_complete(req_id, UV_ENOBUFS);
        delete req;
        return;
    }
    memcpy(req->buf.base, buffer, length);

    uv_write(req, handle_cast<uv_stream_t>(this), &req->buf, 1, [](uv_write_t *uv_req, int status) {
        request *req = static_cast<request*>(uv_req);
        req->socket->write_complete(req->req_id, status);
        delete req;
    });
}

}

}
