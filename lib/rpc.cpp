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

#include "libhdht-private.hpp"

#include <endian.h>
#include <deque>
#include <new>

namespace libhdht
{

namespace rpc
{

namespace impl
{

// Implement a logical buffer that is formed by concatenating multiple buffers
class LogicalBuffer
{
private:
    std::deque<uv::Buffer> m_buffers;
    size_t m_off = 0;
    size_t m_size = 0;

    void compact()
    {
        while (!m_buffers.empty() && m_off >= m_buffers.front().len) {
            m_off -= m_buffers.front().len;
            m_buffers.pop_front();
        }
    }

public:
    void add_buffer(uv::Buffer&& buffer)
    {
        compact();
        m_size += buffer.len;
        m_buffers.emplace_back(std::move(buffer));
    }

    size_t size() const
    {
        return m_size;
    }

    uv::Buffer read(size_t size)
    {
        compact();
        m_size -= size;

        const uv::Buffer& first(m_buffers.front());
        char* start = first.base + m_off;
        if (first.len >= m_off + size) {
            // great, we can make a buffer with no copy
            uv::Buffer buffer((uint8_t*)start, size, false);

            m_off += size;
            return buffer;
        } else {
            // sad, we need to concatenate stuff in a single buffer
            BufferWriter writer;
            writer.reserve(size);

            m_off += size;
            writer.write((uint8_t*)start, first.len, false);
            m_off -= first.len;
            m_buffers.pop_front();

            while (true) {
                const uv::Buffer& front(m_buffers.front());
                size_t to_write = std::min(front.len, m_off);
                writer.write((uint8_t*)front.base, to_write, false);
                if (to_write == front.len) {
                    m_off -= to_write;
                    m_buffers.pop_front();
                } else {
                    break;
                }
            }

            return writer.close();
        }
    }

    void advance(size_t off)
    {
        m_size -= off;
        m_off += off;
        while (m_off > m_buffers.front().len)
        {
            m_off -= m_buffers.front().len;
            m_buffers.pop_front();
        }
    }
};


class Connection : public uv::TCPSocket
{
private:
    Context* m_context;
    std::shared_ptr<Peer> m_peer;
    net::Address m_address;

    // State associated with reading
    LogicalBuffer m_temporary_buffers;
    uint16_t m_opcode = 0;
    uint64_t m_current_request = 0;
    uint64_t m_current_object = 0;
    size_t m_expected_bytes = 0;
    enum class State
    {
        ReadingOpcode,
        ReadingObjectId,
        ReadingError,
        ReadingPayloadSize,
        ReadingPayload
    } m_state = State::ReadingOpcode;

public:
    Connection(Context *ctx) : uv::TCPSocket(ctx->get_event_loop())
    {
        m_context = ctx;
    }
    Connection(Context *ctx, const net::Address& address) : uv::TCPSocket(ctx->get_event_loop())
    {
        m_context = ctx;
        m_address = address;
        connect(address);
    }

    void set_peer(std::shared_ptr<Peer> peer)
    {
        m_peer = peer;
    }
    std::shared_ptr<Peer> get_peer()
    {
        return m_peer;
    }

    void write_request(uint16_t opcode,
                       uint64_t request_id,
                       uint64_t object_id,
                       const uv::Buffer& buffer);
    void write_error(uint64_t request_id,
                     RemoteError error);
    void write_reply(uint64_t request_id,
                     const uv::Buffer& payload);

    virtual void connected(uv::Error err) override;
    virtual void closed() override;
    virtual void read_callback(uv::Error err, uv::Buffer&&) override;
    virtual void write_complete(uint64_t req_id, uv::Error err) override;
};

void
Connection::connected(uv::Error err)
{
    if (err) {
        if (m_address.is_valid()) {
            std::string name(m_address.to_string());
            log(LOG_WARNING, "Failed to connect to %s: %s", name.c_str(), err.what());
        } else {
            log(LOG_WARNING, "Failed to connect: %s", err.what());
        }
        close();
        return;
    }

    m_address = get_peer_name();
    m_context->add_peer_address(m_peer, m_address);
}

void
Connection::closed()
{
    if (m_peer) {
        m_peer->drop_connection(this);
        m_context->remove_peer_address(m_peer, m_address);
    }
    uv::TCPSocket::closed();
}

void
Connection::read_callback(uv::Error err, uv::Buffer&& buffer)
{
    if (err) {
        std::string name(m_address.to_string());
        if (err == UV_EOF) {
            log(LOG_NOTICE, "Connection with peer %s closed", name.c_str());
        } else {
            log(LOG_WARNING, "Read error from peer %s: %s", name.c_str(), err.what());
        }
        close();
        return;
    }

    // ignore
    if (!is_usable())
        return;

    m_temporary_buffers.add_buffer(std::move(buffer));

    bool has_data = true;
    while (has_data) {
        switch (m_state) {
        case State::ReadingOpcode:
            if (m_temporary_buffers.size() >= sizeof(protocol::MessageHeader)) {
                uv::Buffer header_buffer = m_temporary_buffers.read(sizeof(protocol::MessageHeader));
                const protocol::MessageHeader* header = (const protocol::MessageHeader*) header_buffer.base;
                m_opcode = le16toh(header->opcode);
                m_current_request = le64toh(header->request_id);

                if (m_opcode == 0 || m_opcode == protocol::REPLY_FLAG) {
                    log(LOG_ERR, "Invalid request with null opcode");
                    // Close the connection with extreme prejudice
                    close();
                    return;
                }
                if ((m_opcode & ~protocol::REPLY_FLAG) >= (uint16_t)::libhdht::protocol::Opcode::max_opcode) {
                    log(LOG_ERR, "Invalid request opcode");
                    // Close the connection with extreme prejudice
                    close();
                    return;
                }

                if (m_opcode & protocol::REPLY_FLAG) {
                    m_state = State::ReadingError;
                } else {
                    m_state = State::ReadingObjectId;
                }
                break;
            } else {
                has_data = false;
                break;
            }

        case State::ReadingObjectId:
            if (m_temporary_buffers.size() >= sizeof(uint64_t)) {
                uv::Buffer object_id_buffer = m_temporary_buffers.read(sizeof(uint64_t));
                uint64_t *p_object_id = (uint64_t*) object_id_buffer.base;
                uint64_t object_id = le64toh(*p_object_id);

                if (object_id == 0) {
                    log(LOG_ERR, "Invalid request on object 0");
                    // Close the connection with extreme prejudice
                    close();
                    return;
                }

                m_current_object = object_id;
                m_state = State::ReadingPayloadSize;
                break;
            } else {
                has_data = false;
                break;
            }

        case State::ReadingError:
            if (m_temporary_buffers.size() >= sizeof(uint32_t)) {
                uv::Buffer error_code_buffer = m_temporary_buffers.read(sizeof(uint32_t));
                uint32_t *p_error_code = (uint32_t*) error_code_buffer.base;
                uint32_t error_code = le32toh(*p_error_code);

                if (error_code == 0) { // no error!
                    m_state = State::ReadingPayloadSize;
                } else {
                    rpc::RemoteError error(error_code);

                    uint64_t request_id = m_current_request;
                    m_expected_bytes = 0;
                    m_opcode = 0;
                    m_current_request = 0;
                    m_current_object = 0;
                    m_state = State::ReadingOpcode;
                    m_peer->reply_received(request_id, &error, nullptr);
                }
                break;
            } else {
                has_data = false;
                break;
            }

        case State::ReadingPayloadSize:
            if (m_temporary_buffers.size() >= sizeof(uint16_t)) {
                uv::Buffer payload_size_buffer = m_temporary_buffers.read(sizeof(uint16_t));
                uint16_t *p_payload_size = (uint16_t*) payload_size_buffer.base;
                uint16_t payload_size = le16toh(*p_payload_size);

                m_expected_bytes = payload_size;
                m_state = State::ReadingPayload;
                break;
            } else {
                has_data = false;
                break;
            }

        case State::ReadingPayload:
            if (m_temporary_buffers.size() >= m_expected_bytes) {
                uv::Buffer payload = m_temporary_buffers.read(m_expected_bytes);

                if (m_opcode & protocol::REPLY_FLAG) {
                    uint64_t request_id = m_current_request;
                    m_expected_bytes = 0;
                    m_opcode = 0;
                    m_current_request = 0;
                    m_current_object = 0;
                    m_state = State::ReadingOpcode;
                    m_peer->reply_received(request_id, nullptr, &payload);
                } else {
                    uint16_t opcode = m_opcode;
                    uint64_t object_id = m_current_object;
                    uint64_t request_id = m_current_request;
                    m_expected_bytes = 0;
                    m_opcode = 0;
                    m_current_request = 0;
                    m_current_object = 0;
                    m_state = State::ReadingOpcode;
                    m_peer->request_received(opcode, object_id, request_id, payload);
                }
                break;
            } else {
                has_data = false;
                break;
            }
        }
    }
}

void
Connection::write_complete(uint64_t req_id, uv::Error err)
{
    // if this connection was already severed, ignore the error
    if (!is_usable())
        return;

    std::string name(m_address.to_string());
    if (err) {
        log(LOG_WARNING, "Write error to %s: %s", name.c_str(), err.what());
        close();

        m_peer->write_failed(req_id, err);
    } else {
        log(LOG_DEBUG, "Successfully written %s %lu to %s", (req_id & (1ULL<<63) ? "reply" : "request"), req_id & ~(1ULL<<63), name.c_str());
    }
}

void
Connection::write_request(uint16_t opcode,
                          uint64_t request_id,
                          uint64_t object_id,
                          const uv::Buffer& payload)
{
    try {
        BufferWriter header;
        header.reserve(sizeof(protocol::BaseRequest));
        header.write(opcode);
        header.write(request_id);
        header.write(object_id);
        assert(payload.len <= protocol::MAX_PAYLOAD_SIZE);
        header.write(static_cast<uint16_t>(payload.len));

        uv::Buffer buffers[2] = { header.close(), uv::Buffer((uint8_t*)payload.base, payload.len, false) };
        write(request_id, buffers, 2);
    } catch(const uv::Error& err) {
        write_complete(request_id, err);
    }
}

void
Connection::write_reply(uint64_t request_id,
                        const uv::Buffer& payload)
{
    try {
        BufferWriter header;
        header.reserve(sizeof(protocol::BaseResponse));
        header.write(protocol::REPLY_FLAG);
        header.write(request_id);
        header.write(static_cast<uint32_t>(0) /* error code */);
        assert(payload.len <= protocol::MAX_PAYLOAD_SIZE);
        header.write(static_cast<uint16_t>(payload.len));

        uv::Buffer buffers[2] = { header.close(), uv::Buffer((uint8_t*)payload.base, payload.len, false) };
        write(request_id | (1ULL<<63), buffers, 2);
    } catch(const uv::Error& err) {
        write_complete(request_id | (1ULL<<63), err);
    }
}

void
Connection::write_error(uint64_t request_id,
                        RemoteError error)
{
    try {
        BufferWriter header;
        header.reserve(sizeof(protocol::BaseResponse));
        header.write(protocol::REPLY_FLAG);
        header.write(request_id);
        header.write(error.code());
        header.write(static_cast<uint16_t>(0));

        uv::Buffer buffers[1] = { header.close() };
        write(request_id | (1ULL<<63), buffers, 1);
    } catch(const uv::Error& err) {
        write_complete (request_id | (1ULL<<63), err);
    }
}

}

void
Peer::adopt_connection(impl::Connection* connection)
{
    m_available_connections.push_back(connection);
    connection->set_peer(shared_from_this());
    connection->start_reading();
}

void
Peer::drop_connection(impl::Connection* connection)
{
    for (auto it = m_available_connections.begin(); it != m_available_connections.end(); it++) {
        if (*it == connection) {
            m_available_connections.erase(it);
            return;
        }
    }
}

void
Peer::close_all_connections()
{
    for (auto conn : m_available_connections)
        conn->close();
    m_available_connections.clear();
}

impl::Connection*
Peer::get_connection()
{
    for (auto conn : m_available_connections) {
        if (conn->is_usable())
            return conn;
    }

    net::Address address = get_listening_address();
    if (!address.is_valid())
        return nullptr;

    impl::Connection *new_connection = new impl::Connection(m_context, address);
    adopt_connection(new_connection);
    return new_connection;
}

Peer::OutstandingRequest&
Peer::queue_request(uint64_t request_id, RemoteError error, uv::Buffer&& payload, const std::function<void(Error*, const uv::Buffer*)>& callback)
{
    OutstandingRequest req { error, std::move(payload), callback };
    return m_requests.insert(std::make_pair(request_id, std::move(req))).first->second;
}

void
Peer::invoke_request(uint16_t opcode,
                     uint64_t object_id,
                     uv::Buffer&& payload,
                     const std::function<void(Error*, const uv::Buffer*)>& callback)
{
    if (payload.len > protocol::MAX_PAYLOAD_SIZE) {
        rpc::NetworkError err(UV_E2BIG);
        callback(&err, nullptr);
        return;
    }
    impl::Connection *connection = get_connection();
    if (connection == nullptr) {
        rpc::NetworkError err(UV_EAI_NONAME);
        callback(&err, nullptr);
        return;
    }

    uint64_t request_id = m_next_req_id++;
    OutstandingRequest& req = queue_request(request_id, 0, std::move(payload), callback);
    connection->write_request(opcode, request_id, object_id, req.payload);
    // TODO start a retransmission timeout
}

void
Peer::send_error(uint64_t request_id,
                 RemoteError error)
{
    OutstandingRequest& req = queue_request(request_id | (1ULL<<63), error, uv::Buffer(), nullptr);
    impl::Connection *connection = get_connection();
    if (connection == nullptr) {
        // connection was dropped and we can't reconnect, ignore until the other peer connects again
        return;
    }

    connection->write_error(request_id, req.error);
}

void
Peer::send_fatal_error(uint64_t request_id,
                       RemoteError error)
{
    send_error(request_id, error);
    close_all_connections();
}

void
Peer::send_reply(uint64_t request_id,
                 uv::Buffer&& reply)
{
    OutstandingRequest& req = queue_request(request_id | (1ULL<<63), 0, std::move(reply), nullptr);
    impl::Connection *connection = get_connection();
    if (connection == nullptr) {
        // connection was dropped and we can't reconnect, ignore until the other peer connects again
        return;
    }

    connection->write_reply(request_id, req.payload);
}

void
Peer::request_received(uint16_t opcode, uint64_t object_id, uint64_t request_id, const uv::Buffer& payload)
{
    auto it = m_stubs.find(object_id);

    if (it == m_stubs.end()) {
        log(LOG_ERR, "Invalid object id %lu in incoming %s request", object_id, ::libhdht::protocol::get_request_name(opcode));
        send_fatal_error(object_id, EINVAL);
        return;
    }

    // TODO check if this request was already handled
    it->second->dispatch_request(opcode, request_id, payload);
}

void
Peer::reply_received(uint64_t request_id, rpc::RemoteError* error, const uv::Buffer* payload)
{
    auto it = m_requests.find(request_id);

    if (it == m_requests.end()) {
        log(LOG_WARNING, "Received reply to invalid request %lu", request_id);
        return;
    }

    it->second.callback(error, payload);
    m_requests.erase(it);
}

void
Peer::write_failed(uint64_t request_id, uv::Error err)
{

}

void
Peer::add_listening_address(const net::Address& address)
{
    m_addresses.insert(address);
    m_context->add_peer_address(shared_from_this(), address);
}

void
Peer::remove_listening_address(const net::Address& address)
{
    m_addresses.erase(address);
    m_context->remove_peer_address(shared_from_this(), address);
}

namespace impl
{

class Server : public uv::TCPSocket
{
private:
    Context *m_context;
    net::Address m_address;

public:
    Server(Context *ctx, const net::Address& address) :
        uv::TCPSocket(ctx->get_event_loop()),
        m_context(ctx),
        m_address(address)
    {
        listen(address);
        start_reading();
    }
    Server(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(const Server&) = delete;
    Server& operator=(Server&&) = delete;

    const net::Address& get_listening_address() const
    {
        return m_address;
    }

    virtual void new_connection() override
    {
        Connection* connection = new Connection(m_context);
        accept(connection);
        m_context->new_connection(connection);
    }
};

}

Context::Context(uv::Loop& loop) : m_loop(loop)
{}

Context::~Context()
{}

void
Context::add_address(const net::Address& address)
{
    auto socket = std::make_unique<impl::Server>(this, address);
    m_listening_sockets.push_back(std::move(socket));

    log(LOG_INFO, "Listening on address %s", address.to_string().c_str());
}

std::shared_ptr<Peer>
Context::get_peer(const net::Address& address, Context::AddressType type)
{
    auto it = m_known_peers.find(address);
    if (it != m_known_peers.end()) {
        std::shared_ptr<Peer> ptr = it->second.lock();
        if (ptr)
            return ptr;
    }

    std::shared_ptr<Peer> ptr = std::make_shared<Peer>(this);
    for (auto& factory : m_stub_factories)
        factory(ptr);
    if (type == AddressType::Static)
        ptr->add_listening_address(address);
    else
        m_known_peers.insert(std::make_pair(address, ptr));

    return ptr;
}

net::Address
Context::get_listening_address() const
{
    if (m_listening_sockets.empty())
        return net::Address();

    return m_listening_sockets.front()->get_listening_address();
}

void
Context::new_connection(impl::Connection* connection)
{
    try {
        net::Address address = connection->get_peer_name();
        log(LOG_INFO, "New connection from %s", address.to_string().c_str());

        get_peer(address, AddressType::Dynamic)->adopt_connection(connection);
    } catch(std::exception& e) {
        log(LOG_WARNING, "Failed to handle new connection: %s", e.what());
    }
}

void
Context::add_peer_address(std::shared_ptr<Peer> peer, const net::Address& address)
{
    m_known_peers.insert(std::make_pair(address, peer));
}

void
Context::remove_peer_address(std::shared_ptr<Peer> peer, const net::Address& address)
{
    m_known_peers.erase(address);
}

// empty destructor, just to fill in the vtable slot
Stub::~Stub()
{}

}

}
