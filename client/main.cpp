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
#include <iostream>
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
    net::Name peer;

    void help(const char* argv0) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s [-l ADDRESS] -s SERVER\n\n", argv0);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -h         : show this help\n");
        fprintf(stderr, "  -d         : enable debugging (log to stderr instead of syslog)\n");
        fprintf(stderr, "  -l ADDRESS : listen on the given address\n");
        fprintf(stderr, "  -s SERVER  : connect to the given server\n");
    }

    Options(int argc, char* const* argv) {
        int opt;
        bool found_peer = false;
        while ((opt = getopt(argc, argv, ":dl:s:")) >= 0) {
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

            case 's':
                if (found_peer) {
                    fprintf(stderr, "Duplicate option -s\n");
                    help(argv[0]);
                    exit(1);
                }
                peer = net::Name(optarg);
                found_peer = true;
                break;
            }
        }

        if (!found_peer) {
            fprintf(stderr, "Must specify the name of a server to connect to\n");
            help(argv[0]);
            exit(1);
        }

        if (!own_address.is_valid()) {
            own_address = net::Address(protocol::DEFAULT_PORT);
        }
    }
};

using std::cout;
using std::endl;

class Client : private uv::TTY
{
    Options m_opts;
    ClientContext m_ctx;
    uv::Loop& m_event_loop;
    bool m_reading = false;

    void prompt()
    {
        cout << "$ ";
        cout.flush();
        if (!m_reading)
            start_reading();
    }

public:
    Client(int argc, char* const* argv, uv::Loop& event_loop) :
        uv::TTY(event_loop, STDIN_FILENO),
        m_opts(argc, argv),
        m_ctx(event_loop),
        m_event_loop(event_loop)
    {
        try {
            m_ctx.add_address(m_opts.own_address);
            auto addresses = m_opts.peer.resolve_sync();
            if (!addresses.empty())
                m_ctx.set_initial_server(addresses.front());
        } catch(std::runtime_error& e) {
            cout << "Failed to initialize: " << e.what() << endl;
            exit(1);
        }

        cout << "Welcome to HDHT." << endl;
        cout << "Available commands: " << endl;
        cout << "  set-location <lat> <lon>" << endl;
        cout << "  show-location" << endl;
        cout << "  set-metadata <key> <value>" << endl;
        cout << "  show-metadata <key>" << endl;
        cout << "  show-server" << endl;
        cout << "  get-metadata <node_id> <key>" << endl;
        cout << "  quit" << endl;
        prompt();
    }

    virtual void read_line(uv::Error err, uv::Buffer&& line)
    {
        uv::BufferInputStringBuf stringbuf(std::move(line));
        std::istream parser(&stringbuf);

        if (err) {
            // from stdin, the only real error is EOF, just quit in that case
            cout << "Bye" << endl;
            m_event_loop.stop();
        }

        std::string command;
        parser >> command;
        if (command.empty()) {
            prompt();
            return;
        }
        if (command == "set-location") {
            double lat, lon;
            parser >> lat >> lon;
            m_ctx.set_location(GeoPoint2D { lat, lon });
        } else if (command == "show-location") {
            auto pt = m_ctx.get_location();
            cout << "Current Location: " << pt.to_string() << endl;
        } else if (command == "set-metadata") {
            std::string key, value;
            parser >> key >> value;
            m_ctx.set_local_metadata(key, value);
        } else if (command == "show-metadata") {
            std::string key;
            cout << key << " = " << m_ctx.get_local_metadata(key) << endl;
        } else if (command == "show-server") {
            cout << "Current Server: " << m_ctx.get_current_server().to_string() << endl;
        } else if (command == "get-metadata") {
            std::string node_id, key;
            parser >> node_id >> key;
            m_reading = false;
            stop_reading();
            m_ctx.get_remote_metadata(node_id, key, [this, key](rpc::Error *err, const std::string* value) {
                if (err)
                    cout << "Failed: " << err->what() << endl;
                else
                    cout << key << " = " << *value << endl;
                prompt();
            });
            return;
        } else if (command == "quit") {
            cout << "Bye" << endl;
            m_event_loop.stop();
            return;
        } else {
            cout << "Unknown command " << command << endl;
        }
        prompt();
    }
};

int main(int argc, char* const* argv)
{
    libhdht::init();

    {
        libhdht::uv::Loop event_loop;

        // Client will be freed automatically when the stdin handle is closed
        new Client(argc, argv, event_loop);

        event_loop.run();
    }

    libhdht::fini();
}