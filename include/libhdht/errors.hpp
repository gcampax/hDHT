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

#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <cstring>
#include <uv.h>

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

}

namespace rpc
{

class Error : public std::runtime_error
{
protected:
    Error(const char* msg) : std::runtime_error(msg) {}
};

class NetworkError : public Error
{
private:
    uv::Error m_error;

public:
    NetworkError(const uv::Error& error) : Error(error.what()), m_error(error) {}
};

class RemoteError : public Error
{
private:
    uint32_t m_code;

public:
    RemoteError(uint32_t code) : Error(strerror(code)), m_code(code) {}

    uint32_t code() const
    {
        return m_code;
    }
};

}

}
