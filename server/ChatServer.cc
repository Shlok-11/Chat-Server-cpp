#include "ChatServer.h"
#include "../common/Protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

namespace {

// Splits "a:b:c" into ["a","b","c"], but the LAST piece can contain
// further colons (so PMSG:username:hello: there works as expected).
std::vector<std::string> splitCommand(const std::string& payload, int maxParts) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (static_cast<int>(parts.size()) < maxParts - 1) {
        size_t pos = payload.find(':', start);
        if (pos == std::string::npos) break;
        parts.push_back(payload.substr(start, pos - start));
        start = pos + 1;
    }
    parts.push_back(payload.substr(start));
    return parts;
}

}  // namespace

ChatServer::ChatServer(int port) : port_(port) {}

void ChatServer::run() {
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("Failed to bind to port " + std::to_string(port_));
    }

    if (listen(listenFd_, /*backlog=*/64) < 0) {
        throw std::runtime_error("Failed to listen");
    }

    std::cout << "[server] Listening on port " << port_ << "...\n";
    acceptLoop();
}

void ChatServer::acceptLoop() {
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            std::cerr << "[server] accept() failed, continuing\n";
            continue;
        }

        // Detached thread-per-connection: the thread cleans up after
        // itself (closes the socket, unregisters the username) when the
        // client disconnects.
        std::thread(&ChatServer::handleClient, this, clientFd).detach();
    }
}

void ChatServer::handleClient(int clientFd) {
    std::string username;
    std::string payload;

    // --- Handshake: first message must be CONNECT:<username> ---
    if (!protocol::recvFramed(clientFd, payload)) {
        close(clientFd);
        return;
    }

    auto parts = splitCommand(payload, 2);
    if (parts.size() != 2 || parts[0] != "CONNECT" || parts[1].empty()) {
        protocol::sendFramed(clientFd, "ERR:First message must be CONNECT:<username>");
        close(clientFd);
        return;
    }

    username = parts[1];
    if (!registerClient(username, clientFd)) {
        protocol::sendFramed(clientFd, "ERR:Username already taken");
        close(clientFd);
        return;
    }

    protocol::sendFramed(clientFd, "OK:Welcome, " + username + "!");
    broadcast(username, username + " joined the chat", clientFd);
    std::cout << "[server] " << username << " connected\n";

    // --- Main command loop ---
    while (protocol::recvFramed(clientFd, payload)) {
        if (payload.rfind("MSG:", 0) == 0) {
            std::string text = payload.substr(4);
            broadcast(username, text, clientFd);

        } else if (payload.rfind("PMSG:", 0) == 0) {
            auto pmsgParts = splitCommand(payload.substr(5), 2);
            if (pmsgParts.size() == 2) {
                bool ok = sendPrivate(username, pmsgParts[0], pmsgParts[1]);
                if (!ok) {
                    protocol::sendFramed(clientFd, "ERR:User '" + pmsgParts[0] + "' not online");
                }
            }

        } else if (payload == "LIST") {
            protocol::sendFramed(clientFd, "USERLIST:" + listUsersCsv());

        } else if (payload == "QUIT") {
            break;

        } else {
            protocol::sendFramed(clientFd, "ERR:Unknown command");
        }
    }

    unregisterClient(username);
    broadcast(username, username + " left the chat", clientFd);
    std::cout << "[server] " << username << " disconnected\n";
    close(clientFd);
}

bool ChatServer::registerClient(const std::string& username, int clientFd) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    if (clients_.count(username)) {
        return false;
    }
    clients_[username] = clientFd;
    return true;
}

void ChatServer::unregisterClient(const std::string& username) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.erase(username);
}

void ChatServer::broadcast(const std::string& fromUser, const std::string& text, int excludeFd) {
    std::string msg = "BROADCAST:" + fromUser + ":" + text;
    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (const auto& [name, fd] : clients_) {
        if (fd == excludeFd) continue;
        protocol::sendFramed(fd, msg);
    }
}

bool ChatServer::sendPrivate(const std::string& fromUser, const std::string& toUser, const std::string& text) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = clients_.find(toUser);
    if (it == clients_.end()) {
        return false;
    }
    protocol::sendFramed(it->second, "PRIVATE:" + fromUser + ":" + text);
    return true;
}

std::string ChatServer::listUsersCsv() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::ostringstream oss;
    bool first = true;
    for (const auto& [name, fd] : clients_) {
        if (!first) oss << ",";
        oss << name;
        first = false;
    }
    return oss.str();
}
