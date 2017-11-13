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

#ifdef HAVE_SYSTEMD
#include <systemd/sd-journal.h>
#endif

namespace libhdht {

void init()
{
    // initialize the library
    // eg initialize gettext, or gmp, or openssl, or whatever else we need
}

void fini()
{
    // release any resource associated with the library
}

#ifdef HAVE_SYSTEMD
static int(*logger)(int, const char*, va_list) = sd_journal_printv;
#else
static int(*logger)(int, const char*, va_list) = vsyslog;
#endif

void
set_log_function(int(*function)(int, const char*, va_list))
{
    logger = function;
}

void log(int level, const char* msg, ...)
{
    va_list va;
    va_start(va, msg);
    logger(level, msg, va);
    va_end(va);
}

}