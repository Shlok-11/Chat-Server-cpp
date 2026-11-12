#include "ChatClient.h"
#include "../common/Protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <thread>

ChatClient::ChatClient(const std::string& host, int port, std::string username)
    : host_(host), port_(port), username_(std::move(username)) {}

void ChatClient::printHelp() const {
    std::cout <<
        "Commands:\n"
        "  <text>                just type to broadcast to everyone\n"
        "  /msg <user> <text>    send a private message\n"
        "  /list                 show online users\n"
        "  /quit                 disconnect\n";
}

void ChatClient::receiveLoop() {
    std::string payload;
    while (protocol::recvFramed(sockFd_, payload)) {
        if (payload.rfind("BROADCAST:", 0) == 0) {
            auto rest = payload.substr(10);
            auto sep = rest.find(':');
            std::cout << "\r[" << rest.substr(0, sep) << "] " << rest.substr(sep + 1) << "\n> " << std::flush;

        } else if (payload.rfind("PRIVATE:", 0) == 0) {
            auto rest = payload.substr(8);
            auto sep = rest.find(':');
            std::cout << "\r(private) [" << rest.substr(0, sep) << "] " << rest.substr(sep + 1) << "\n> " << std::flush;

        } else if (payload.rfind("SYSTEM:", 0) == 0) {
            std::cout << "\r*** " << payload.substr(7) << "\n> " << std::flush;

        } else if (payload.rfind("USERLIST:", 0) == 0) {
            std::cout << "\rOnline users: " << payload.substr(9) << "\n> " << std::flush;

        } else if (payload.rfind("ERR:", 0) == 0) {
            std::cout << "\r[error] " << payload.substr(4) << "\n> " << std::flush;

        } else if (payload.rfind("OK:", 0) == 0) {
            std::cout << "\r" << payload.substr(3) << "\n> " << std::flush;
        }
    }
    std::cout << "\n[client] Disconnected from server.\n";
    std::exit(0);
}

void ChatClient::run() {
    sockFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd_ < 0) {
        std::cerr << "[client] Failed to create socket\n";
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<uint16_t>(port_));
    if (inet_pton(AF_INET, host_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "[client] Invalid host address: " << host_ << "\n";
        return;
    }

    if (connect(sockFd_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "[client] Could not connect to " << host_ << ":" << port_ << "\n";
        return;
    }

    if (!protocol::sendFramed(sockFd_, "CONNECT:" + username_)) {
        std::cerr << "[client] Handshake failed\n";
        return;
    }

    std::string reply;
    if (!protocol::recvFramed(sockFd_, reply) || reply.rfind("OK:", 0) != 0) {
        std::cerr << "[client] Server rejected connection: " << reply << "\n";
        close(sockFd_);
        return;
    }

    std::cout << reply.substr(3) << "\n";
    printHelp();

    std::thread receiver(&ChatClient::receiveLoop, this);
    receiver.detach();

    std::string line;
    std::cout << "> " << std::flush;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> " << std::flush;
            continue;
        }

        if (line == "/quit") {
            protocol::sendFramed(sockFd_, "QUIT");
            break;

        } else if (line == "/list") {
            protocol::sendFramed(sockFd_, "LIST");

        } else if (line.rfind("/msg ", 0) == 0) {
            std::istringstream iss(line.substr(5));
            std::string toUser;
            iss >> toUser;
            std::string text;
            std::getline(iss, text);
            if (!text.empty() && text[0] == ' ') text = text.substr(1);
            protocol::sendFramed(sockFd_, "PMSG:" + toUser + ":" + text);

        } else {
            protocol::sendFramed(sockFd_, "MSG:" + line);
        }

        std::cout << "> " << std::flush;
    }

    close(sockFd_);
}
