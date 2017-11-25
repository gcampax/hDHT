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

namespace libhdht {

namespace protocol {

static const char* request_name_table[] = {
    "invalid"
#define begin_class(name)
#define end_class
#define request(return_type, opcode, ...) #opcode
#include "protocol.inc.hpp"
    , nullptr /* max_opcode */
#undef request
#undef end_class
#undef begin_class
};

const char*
get_request_name(uint16_t opcode)
{
    return request_name_table[opcode];
}

namespace impl
{

template <typename Type, typename Callback, class Tuple, std::size_t... I>
void dispatch_helper2(Type* object, Callback callback, uint64_t request_id, Tuple&& args, std::index_sequence<I...>)
{
    (object->*callback)(request_id, std::get<I>(std::forward<Tuple>(args))...);
}

template<typename Type, typename Callback, typename... Args>
void dispatch_helper(Type* object, Callback callback, uint64_t request_id, std::tuple<Args...>&& args)
{
    dispatch_helper2(object, callback, request_id, std::move(args), std::index_sequence_for<Args...>());
}

template<typename Type, typename Callback, typename SingleArg>
void dispatch_helper(Type* object, Callback callback, uint64_t request_id, SingleArg&& arg)
{
    (object->*callback)(request_id, std::move(arg));
}

}

#define begin_class(name) \
    void \
    name##Stub::dispatch_request(int16_t opcode, uint64_t request_id, const uv::Buffer& buffer) {\
        std::shared_ptr<rpc::Peer> peer = get_peer(); \
        if (!peer) {\
            log(LOG_ERR, "Peer disappeared before request could be handled\n");\
            return;\
        }\
        rpc::BufferReader reader(buffer);\
        switch(opcode) {
#define end_class \
        default:\
            log(LOG_ERR, "Invalid request %d", opcode); \
            reply_fatal_error(request_id, ENOSYS);\
        } /* close switch */ \
    } /* close method */
#define request(return_type, opcode, ...) \
    case (uint16_t)(Opcode::opcode): \
        try {\
            impl::dispatch_helper(this, &stub_type::handle_##opcode, request_id, rpc::impl::pack_marshaller<__VA_ARGS__>::from_buffer(*peer, reader));\
        } catch(rpc::RemoteError e) { \
            reply_error(request_id, e);\
        } catch(rpc::ReadError e) {\
            log(LOG_ERR, "Failed to demarshal incoming request");\
            reply_fatal_error(request_id, EINVAL);\
        }\
        break;
#include "protocol.inc.hpp"
#undef request
#undef end_class
#undef begin_class

}
}


