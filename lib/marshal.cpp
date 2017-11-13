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

#include <endian.h>
#include <cassert>

namespace libhdht
{

namespace rpc
{

static const size_t INITIAL_CAPACITY = 8;

BufferWriter::BufferWriter() : m_storage(nullptr), m_length(0), m_capacity(0), m_closed(false)
{
    m_storage = (uint8_t*) malloc(INITIAL_CAPACITY);
    if (m_storage != nullptr)
        m_capacity = INITIAL_CAPACITY;
}

BufferWriter::~BufferWriter()
{
    free(m_storage);
}

uv::Buffer
BufferWriter::close()
{
    assert(!m_closed);
    m_closed = true;

    uint8_t* buffer = m_storage;
    size_t length = m_length;
    m_storage = nullptr;
    m_capacity = 0;
    m_length = 0;

    return uv::Buffer(buffer, length, true);
}

#if BYTE_ORDER == BIG_ENDIAN
static void
bytereverse(uint8_t* buffer, size_t length)
{
    for (size_t i = 0; i < length/2; i++) {
        uint8_t tmp = buffer[i];
        buffer[i] = buffer[length-1-i];
        buffer[length-1-i] = tmp;
    }
}
#endif

void
BufferWriter::write(const uint8_t* buffer, size_t length, bool adjust_endian)
{
    assert(!m_closed);

    // can't allocate memory if it would overflow size_t
    if (m_length + length < m_length)
        throw std::bad_alloc();

    if (m_capacity < m_length + length) {
        size_t new_capacity = m_capacity*2;
        while (new_capacity < m_length + length) {
            if (new_capacity * 2 > new_capacity)
                new_capacity *= 2;
            else
                new_capacity = m_length + length;
        }
        uint8_t* new_storage = (uint8_t*) realloc(m_storage, new_capacity);
        if (new_storage == nullptr && new_capacity > m_length + length) {
            new_capacity = m_length + length;
            new_storage = (uint8_t*) realloc(m_storage, new_capacity);
        }
        if (new_storage == nullptr)
            throw std::bad_alloc();
        m_storage = new_storage;
        m_capacity = new_capacity;
    }

    memcpy(m_storage + m_length, buffer, length);
#if BYTE_ORDER == BIG_ENDIAN
    if (adjust_endian)
        bytereverse(m_storage + m_length, length);
#endif
    m_length += length;
}

}
}
