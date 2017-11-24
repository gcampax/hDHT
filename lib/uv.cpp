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
#include <libhdht/uv.hpp>

#include <new>

namespace libhdht
{

namespace uv
{

TCPSocket::TCPSocket(uv_loop_t* loop)
{
    uv_tcp_init(loop, this);
}

TCPSocket::~TCPSocket()
{
    if (!uv_is_closing(handle_cast<uv_handle_t>(this)))
        std::terminate();
}

net::Address
TCPSocket::get_peer_name() const
{
    net::Address address;
    int size = 0;
    Error::check(uv_tcp_getpeername(this, address.get(), &size));
    return address;
}

void
TCPSocket::close()
{
    uv_close(handle_cast<uv_handle_t>(this), [](uv_handle_t* handle) {
        handle_downcast(handle)->closed();
    });
}

void
TCPSocket::connect(const net::Address& address)
{
    auto req = new uv_connect_t;

    Error::check(uv_tcp_connect(req, this, (sockaddr*)(address.get()), [](uv_connect_t *req, int status) {
        handle_downcast(req->handle)->connected(status);
        delete req;
    }));
}

void
TCPSocket::listen(const net::Address& address)
{
    Error::check(uv_tcp_bind(this, address.get(), 0));
    Error::check(uv_listen(handle_cast<uv_stream_t>(this), 0, [](uv_stream_t* server, int status) {
        TCPSocket *self = handle_downcast(server);
        if (status >= 0) {
            self->new_connection();
        } else {
            self->listen_error(status);
        }
    }));
}

void
TCPSocket::accept(TCPSocket *client)
{
    Error::check(uv_accept(handle_cast<uv_stream_t>(this), handle_cast<uv_stream_t>(client)));
    connected(0);
}

static void
alloc_memory(uv_handle_t*, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = (char*)malloc(suggested_size);
    buf->len = buf->base ? suggested_size : 0;
}

void
TCPSocket::start_reading()
{
    Error::check(uv_read_start(handle_cast<uv_stream_t>(this), alloc_memory, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* uv_buffer) {
        if (nread == 0) {
            // EAGAIN
            free(uv_buffer->base);
            return;
        }
        uv::Error error(nread < 0 ? nread : 0);
        uv::Buffer buffer;

        if (nread > 0)
            buffer = uv::Buffer((const uint8_t*)uv_buffer->base, nread, true);
        else
            free(uv_buffer->base);
        handle_downcast(stream)->read_callback(error, std::move(buffer));
    }));
}

void
TCPSocket::write(uint64_t req_id, const Buffer* buffers, size_t nbuffers)
{
    struct request : uv_write_t
    {
        std::vector<Buffer> buf;
        std::vector<uv_buf_t> uv_buf;
        uint64_t req_id;
    };
    auto req = new (std::nothrow) request();
    if (req == nullptr) {
        write_complete(req_id, UV_ENOBUFS);
        return;
    }

    req->req_id = req_id;
    req->buf.reserve(nbuffers);
    req->uv_buf.reserve(nbuffers);
    for (size_t i = 0; i < nbuffers; i++) {
        const uv::Buffer& copy_from(buffers[i]);
        if (!copy_from)
            continue;
        req->buf.emplace_back(copy_from.clone());
        if (!req->buf.back()) {
            write_complete(req_id, UV_ENOBUFS);
            delete req;
            return;
        }
        req->uv_buf.emplace_back(req->buf.back());
    }

    Error::check(uv_write(req, handle_cast<uv_stream_t>(this), &req->uv_buf[0], req->uv_buf.size(), [](uv_write_t *uv_req, int status) {
        request *req = static_cast<request*>(uv_req);
        handle_downcast(req->handle)->write_complete(req->req_id, status);
        delete req;
    }));
}

}

}
