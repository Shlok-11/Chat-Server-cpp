#include "Protocol.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

// Reads exactly `len` bytes into `buf`, looping over partial reads.
// Returns false on EOF (peer closed) or error.
bool readExact(int sockfd, char* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::recv(sockfd, buf + total, len - total, 0);
        if (n <= 0) {
            // n == 0  -> peer closed the connection cleanly
            // n  < 0  -> socket error (e.g. ECONNRESET)
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

// Writes exactly `len` bytes from `buf`, looping over partial writes.
bool writeExact(int sockfd, const char* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::send(sockfd, buf + total, len - total, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

}  // namespace

namespace protocol {

bool sendFramed(int sockfd, const std::string& payload) {
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    if (!writeExact(sockfd, reinterpret_cast<const char*>(&len), sizeof(len))) {
        return false;
    }
    if (payload.empty()) {
        return true;
    }
    return writeExact(sockfd, payload.data(), payload.size());
}

bool recvFramed(int sockfd, std::string& out) {
    uint32_t lenNetworkOrder = 0;
    if (!readExact(sockfd, reinterpret_cast<char*>(&lenNetworkOrder), sizeof(lenNetworkOrder))) {
        return false;
    }

    uint32_t len = ntohl(lenNetworkOrder);

    // Guard against a malformed/hostile length header causing a huge allocation.
    constexpr uint32_t kMaxMessageSize = 1 << 20;  // 1 MB
    if (len > kMaxMessageSize) {
        return false;
    }

    if (len == 0) {
        out.clear();
        return true;
    }

    std::vector<char> buf(len);
    if (!readExact(sockfd, buf.data(), len)) {
        return false;
    }

    out.assign(buf.data(), len);
    return true;
}

}  // namespace protocol
