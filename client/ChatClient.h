#pragma once
#include <string>

// A simple CLI chat client.
//
// Uses two threads: the main thread reads lines from stdin and sends
// commands; a background thread blocks on recvFramed() and prints
// whatever the server pushes (broadcasts, private messages, system
// notices) as it arrives — so incoming messages show up in real time
// even while you're mid-way through typing something.
class ChatClient {
public:
    ChatClient(const std::string& host, int port, std::string username);

    // Connects, performs the CONNECT handshake, then blocks running the
    // send/receive loop until the user types /quit or the connection drops.
    void run();

private:
    void receiveLoop();
    void printHelp() const;

    std::string host_;
    int port_;
    std::string username_;
    int sockFd_ = -1;
};
