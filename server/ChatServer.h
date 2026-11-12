#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>

// A multi-threaded TCP chat server.
//
// Concurrency model: one dedicated thread per connected client
// ("thread-per-connection"). Simple to reason about and plenty for a
// resume-scale chat server; a production system handling thousands of
// idle connections would switch to an event loop (epoll) + thread pool
// to avoid per-thread stack overhead — noted in the README as a
// deliberate trade-off.
//
// Shared state (the username -> socket map) is protected by a single
// mutex. Every client thread only holds the lock for the brief window
// needed to read/update that map or to write a broadcast message.
class ChatServer {
public:
    explicit ChatServer(int port);

    // Blocks forever, accepting and servicing clients.
    void run();

private:
    void acceptLoop();
    void handleClient(int clientFd);

    // Registers `username` -> `clientFd`. Returns false if the name is
    // already taken.
    bool registerClient(const std::string& username, int clientFd);
    void unregisterClient(const std::string& username);

    void broadcast(const std::string& fromUser, const std::string& text, int excludeFd);
    bool sendPrivate(const std::string& fromUser, const std::string& toUser, const std::string& text);
    std::string listUsersCsv();

    int port_;
    int listenFd_ = -1;

    std::mutex clientsMutex_;
    std::unordered_map<std::string, int> clients_;  // username -> socket fd
};
