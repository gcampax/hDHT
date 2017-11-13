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

#include <cstdio>
#include <unistd.h>

using namespace libhdht;

static int debug_logger(int priority, const char *msg, va_list va)
{
    static const char *priority_str[] = {
        "emerg", "alert", "crit", "err", "warning", "notice", "info", "debug"
    };
    fprintf(stderr, "%s: ", priority_str[priority]);
    vfprintf(stderr, msg, va);
    fprintf(stderr, "\n");
    return 0;
}

struct Options
{
    net::Address own_address;
    std::vector<net::Name> known_peers;

    void help(const char* argv0) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s -l ADDRESS [-p PEER]*\n\n", argv0);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -h         : show this help\n");
        fprintf(stderr, "  -d         : enable debugging (log to stderr instead of syslog)\n");
        fprintf(stderr, "  -l ADDRESS : listen on the given address\n");
        fprintf(stderr, "  -p PEER    : connect to the given peer\n");
    }

    Options(int argc, char* const* argv) {
        int opt;
        while ((opt = getopt(argc, argv, ":dl:p:")) >= 0) {
            switch(opt) {
            case '?':
                fprintf(stderr, "Invalid option %c\n", optopt);
                help(argv[0]);
                exit(1);

            case ':':
                fprintf(stderr, "Option %c expects an argument\n", optopt);
                help(argv[0]);
                exit(1);

            case 'd':
                set_log_function(debug_logger);
                break;

            case 'h':
                help(argv[0]);
                exit(0);

            case 'l':
                try {
                    own_address = net::Address(optarg);
                } catch(const net::Error& e) {
                    fprintf(stderr, "Invalid argument to -l: %s\n", e.what());
                    help(argv[0]);
                    exit(1);
                }
                break;

            case 'p':
                known_peers.emplace_back(optarg);
                break;
            }
        }

        if (!own_address.is_valid()) {
            own_address = net::Address(protocol::DEFAULT_PORT);
        }
    }
};

int main(int argc, char* const* argv)
{
    libhdht::init();

    Options opts(argc, argv);
    {
        libhdht::uv::Loop event_loop;

        ServerContext ctx(event_loop);
        try {
            ctx.add_address(opts.own_address);
            for (auto& peer : opts.known_peers) {
                auto addresses = peer.resolve_sync();
                if (!addresses.empty())
                    ctx.add_peer(addresses.front());
            }
            ctx.start();
        } catch(std::runtime_error& e) {
            log(LOG_EMERG, "Failed to initialize daemon: %s", e.what());
            exit(1);
        }

        event_loop.run();
    }

    libhdht::fini();
}
