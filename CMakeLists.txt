cmake_minimum_required(VERSION 3.16)
project(TCPChatServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(chat_server
    server/main.cc
    server/ChatServer.cc
    common/Protocol.cc
)
target_link_libraries(chat_server PRIVATE Threads::Threads)

add_executable(chat_client
    client/main.cc
    client/ChatClient.cc
    common/Protocol.cc
)
target_link_libraries(chat_client PRIVATE Threads::Threads)
