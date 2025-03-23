// #pragma once

#include <chrono>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <string>

namespace simple_socket {

    inline void init() {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }

    inline void cleanup() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    inline int close_socket(int sock) {
#ifdef _WIN32
        return closesocket(sock);
#else
        return close(sock);
#endif
    }

    inline int create_client_socket(const std::string &ip, int port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &server.sin_addr);
        connect(sock, (sockaddr *)&server, sizeof(server));
        return sock;
    }

} // namespace simple_socket

// -----------------------------------------------

int main() {
    using namespace simple_socket;

    init();

    int sock = create_client_socket("127.0.0.1", 4712);

    if (sock < 0) {
        std::cerr << "Failed to connect.\n";
        return 1;
    }

    auto send_and_recv = [&](const std::string &msg) {
        send(sock, msg.c_str(), msg.size(), 0);
        char buf[1024] = {};
        int len = recv(sock, buf, sizeof(buf) - 1, 0);
        if (len > 0) {
            std::cout << "Server: " << std::string(buf, len);
        }
    };

    std::cout << "Sending: PING\n";
    send_and_recv("PING\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Sending: HELLO\n";
    send_and_recv("HELLO\n");

    close_socket(sock);
    cleanup();
    return 0;
}
