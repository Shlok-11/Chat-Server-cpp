#include "ChatClient.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 5555;
    std::string username;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <username> [host] [port]\n";
        return 1;
    }
    username = argv[1];
    if (argc >= 3) host = argv[2];
    if (argc >= 4) port = std::atoi(argv[3]);

    ChatClient client(host, port, username);
    client.run();
    return 0;
}
