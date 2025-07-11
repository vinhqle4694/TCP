cmake_minimum_required(VERSION 3.10)

project(TcpExamples CXX)

# Common compile options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use the targets from the parent project
# No need to find_package since we're building as part of the main project

# Example executables
add_executable(tcp_client_example tcp_client_example.cpp)
target_link_libraries(tcp_client_example tcp::tcp_static)

add_executable(tcp_server_example tcp_server_example.cpp)
target_link_libraries(tcp_server_example tcp::tcp_static)

add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server tcp::tcp_static)

add_executable(echo_client echo_client.cpp)
target_link_libraries(echo_client tcp::tcp_static)

add_executable(chat_server chat_server.cpp)
target_link_libraries(chat_server tcp::tcp_static)

add_executable(chat_client chat_client.cpp)
target_link_libraries(chat_client tcp::tcp_static)

# Installation
install(TARGETS 
    tcp_client_example 
    tcp_server_example 
    echo_server 
    echo_client 
    chat_server 
    chat_client
    DESTINATION bin/examples
)
