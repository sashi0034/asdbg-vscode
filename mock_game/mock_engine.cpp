#pragma once

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

void main() {
}
