#pragma once
#include <string>

// A minimal length-prefixed framing protocol built directly on top of TCP.
//
// TCP is a byte stream, not a message stream — a single send() on one side
// can arrive as multiple recv() calls on the other, or multiple sends can
// arrive coalesced into one recv(). To turn the stream back into discrete
// application messages, every message is preceded by a 4-byte big-endian
// length header:
//
//     [ 4-byte length N ][ N bytes of UTF-8 payload ]
//
// The payload itself is a simple colon-delimited command, e.g.:
//     "CONNECT:kuldeep"
//     "MSG:hello everyone"
//     "PMSG:amit:hey, got a sec?"
//     "LIST"
//     "QUIT"
//
// Server -> client payloads:
//     "OK:<info>"
//     "ERR:<reason>"
//     "BROADCAST:<username>:<text>"
//     "PRIVATE:<username>:<text>"
//     "USERLIST:<comma,separated,names>"
//     "SYSTEM:<text>"                      (join/leave notifications)

namespace protocol {

// Sends `payload` as one framed message on `sockfd`.
// Returns false if the connection was closed or a write error occurred.
bool sendFramed(int sockfd, const std::string& payload);

// Blocks until one full framed message is received on `sockfd`.
// Returns false if the peer closed the connection or an error occurred
// (in which case `out` should be treated as invalid).
bool recvFramed(int sockfd, std::string& out);

}  // namespace protocol
