# hDHT
Using Hilbert curves and DHTs to implement location queries

## Installation
To build hDHT you will need a compiler that supports C++ 14.
A recent version of gcc or clang is preferred. In particular, gcc 5.3 (as shipped in Ubuntu 16.04 LTS) is known **not** to work.

hDHT depends on libuv (libuv.pc) for asynchronous IO.
Optionally, if the systemd libraries (libsystemd.pc) are present, hDHT will be compiled with support for systemd journal logging.

To build, do:
```
mkdir build/
cd build/
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
sudo make install
```

This will install `/usr/bin/hdhtd`, the server, and `/usr/bin/hdhtd-cli`, the command line client.

## Running the server

```
hdhtd -l 0.0.0.0:<PORT> [-p <PEER>]*
```

When running the server, you should pass the `-l` option to choose the address and port to listen on. By default, the server listens on port `7777` on all interfaces. You can pass the `-l` multiple times to listen on multiple addresses.

The `-p` option provides the initial set of peers to the server. If you don't give any, the server will start a new empty DHT, and assume control of the whole range.

Use the `-d` option to enable debugging.

## Running the client

```
hdht-cli -s <SERVER> -l 0.0.0.0:<PORT>
```

To run the client you must pass the `-s` option to choose the server to initially connect to. The client will contact this server to find out who to register to, based on the client's location.
