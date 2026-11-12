# Multi-threaded TCP Chat Server (C++)

A real-time chat server and CLI client built from scratch on raw POSIX
sockets — no networking framework, no external library for the
transport layer. Written to demonstrate core systems/networking
concepts that come up directly in backend and infrastructure
interviews: TCP stream framing, concurrent connection handling, and
thread-safe shared state.

---

## What is this?

A TCP server that lets multiple clients connect at once and:
- broadcast messages to everyone in the chat,
- send private (1-to-1) messages,
- see who's currently online,
- get join/leave notifications in real time.

It ships with both halves: `chat_server` (the server binary) and
`chat_client` (a CLI client you run in a separate terminal per user).
Everything is implemented directly against the OS socket API
(`socket()`, `bind()`, `listen()`, `accept()`, `send()`, `recv()`) —
nothing is delegated to a library like Boost.Asio or a WebSocket
framework, so every layer of the networking stack is visible and
inspectable in the code.

## Why this project is useful (what it actually demonstrates)

This isn't a toy "echo server" — it deliberately confronts three
problems every real network service has to solve:

1. **TCP has no message boundaries.** It's a byte *stream*, not a
   sequence of discrete messages. A naive chat implementation that
   does one `send()` per message and assumes one matching `recv()` on
   the other side will silently corrupt data the moment a message
   gets split across two `recv()` calls, or two messages get glued
   together into one. This project fixes that properly with a custom
   **length-prefixed framing protocol**: every message is preceded by
   a 4-byte big-endian length header, and `readExact()`/`writeExact()`
   loop until the exact number of bytes has actually been transferred
   (see `common/Protocol.cc`). This is the same technique real
   protocols (HTTP/2, gRPC, Kafka's wire protocol) use under the hood.

2. **Handling many clients at once, safely.** The server spins up one
   `std::thread` per connected client and protects the single piece of
   shared state (the `username -> socket fd` map) with a `std::mutex`.
   This is a direct, hands-on demonstration of concurrent programming:
   race conditions, critical sections, and why a broadcast to N
   clients has to hold the lock for the whole iteration.

3. **Designing a small application-level protocol.** On top of the
   raw framing, there's a real (if minimal) protocol with a handshake,
   commands, and structured responses — the same shape as designing
   any RPC or chat protocol at a larger scale.

**Where this is useful beyond "it's a resume project":** the exact
same framing + thread-per-connection pattern is the starting point for
building any custom TCP service — a game server, an IoT telemetry
collector, a custom pub/sub broker, a proprietary trading protocol,
etc. Understanding it from first principles (instead of only ever
having used a framework) is what interviewers are actually probing for
when they ask "how does TCP guarantee message delivery" or "how would
you design a chat backend."

## How to use it

### 1. Prerequisites
- Linux or macOS (uses POSIX sockets — `sys/socket.h`, `arpa/inet.h`)
- CMake ≥ 3.16
- A C++17 compiler (g++/clang++)
- `pthread` (linked automatically via `find_package(Threads)`)

### 2. Build
```bash
git clone https://github.com/Kuldeepcoder2006/tcp-chat-server-cpp.git
cd tcp-chat-server-cpp
mkdir build && cd build
cmake ..
make -j$(nproc)
```
This produces two executables in `build/`: `chat_server` and `chat_client`.

### 3. Run
```bash
# Terminal 1 — start the server, listening on port 5555
./chat_server 5555

# Terminal 2 — connect as "kuldeep"
./chat_client kuldeep 127.0.0.1 5555

# Terminal 3 — connect as "amit"
./chat_client amit 127.0.0.1 5555
```

### 4. Example session
Once connected, a client can type:
```
> hello everyone
```
This is broadcast to every other connected client. Other commands:
```
/msg amit hey, got a sec?      -> private message, only "amit" sees it
/list                          -> print everyone currently online
/quit                          -> disconnect cleanly
```
Every other terminal shows join/leave notifications automatically:
```
*** amit joined the chat
[amit] hello everyone
(private) [amit] hey, got a sec?
*** amit left the chat
```

### 5. Running it over an actual network (not just localhost)
Start the server on a machine with a reachable IP/port (e.g. an EC2
box, port 5555 open in the security group), then connect from any
other machine:
```bash
./chat_client kuldeep <server-public-ip> 5555
```

## Architecture
```
common/Protocol.{h,cc}   - length-prefixed send/recv framing (shared by both sides)
server/ChatServer.{h,cc} - accept loop, per-client thread, broadcast/private logic
server/main.cc           - server entry point
client/ChatClient.{h,cc} - connects, spawns a receiver thread, runs a send loop
client/main.cc           - client entry point
```

## Wire protocol
Every message on the wire: `[4-byte big-endian length][UTF-8 payload]`.

| Direction | Payload | Meaning |
|---|---|---|
| C→S | `CONNECT:<username>` | handshake, must be the first message |
| C→S | `MSG:<text>` | broadcast to everyone |
| C→S | `PMSG:<user>:<text>` | private message |
| C→S | `LIST` | request online users |
| C→S | `QUIT` | graceful disconnect |
| S→C | `OK:<info>` | handshake accepted |
| S→C | `ERR:<reason>` | e.g. username taken, unknown command |
| S→C | `BROADCAST:<user>:<text>` | a public message from `user` |
| S→C | `PRIVATE:<user>:<text>` | a DM from `user` |
| S→C | `USERLIST:<csv>` | response to `LIST` |

## Known trade-offs (worth raising proactively in an interview)
- **Thread-per-connection** is simple and fine at chat-app scale, but
  doesn't scale to tens of thousands of idle connections — an
  `epoll`/`kqueue`-based event loop with a bounded worker pool would be
  the production answer (the same trade-off Node.js vs. Apache prefork
  makes at a higher level).
- **No TLS.** Traffic is plaintext; a real deployment would wrap the
  socket in OpenSSL/`SSL_accept` or put it behind a TLS-terminating proxy.
- **No message persistence** — this is live chat only, no history is
  stored server-side. Adding SQLite for offline message delivery would
  be a natural next step.

## Possible extensions
- Message history / offline delivery (SQLite-backed)
- Chat rooms/channels instead of one global broadcast
- TLS support
- Switch to epoll for scalability, with a benchmark comparing the two
  under a high-connection-count load test
