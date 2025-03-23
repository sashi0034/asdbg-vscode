#pragma once

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

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
    if (connect(sock, (sockaddr *)&server, sizeof(server)) < 0) {
        return -1;
    }

    return sock;
}

} // namespace simple_socket

// C++ 側で受信したブレイクポイント情報を保持するマップ
// key: ファイルパス、value: (行, カラム)のペアのリスト

struct Breakpoint {
    std::string filepath;
    int line;
};

std::map<std::string, std::vector<Breakpoint>> g_breakpoints;

void parseBreakpoints(const std::string &data) {
    std::istringstream iss(data);
    std::string line;
    bool inBpSection = false;
    while (std::getline(iss, line)) {
        if (line == "BREAKPOINTS") {
            inBpSection = true;
            continue;
        }
        if (line == "END_BREAKPOINTS") {
            inBpSection = false;
            break;
        }
        if (inBpSection) {
            // 1行の形式: filepath,line,column
            std::istringstream lineStream(line);
            std::string filepath, lineStr, colStr;
            if (std::getline(lineStream, filepath, ',') &&
                std::getline(lineStream, lineStr, ',') &&
                std::getline(lineStream, colStr)) {
                try {
                    int lineNum = std::stoi(lineStr);
                    g_breakpoints[filepath].push_back({
                        .filepath = filepath,
                        .line = lineNum,
                    });

                    std::cout << "Parsed breakpoint: " << filepath << ", "
                              << lineNum << std::endl;
                } catch (...) {
                    std::cerr << "Failed to parse breakpoint: " << line
                              << std::endl;
                }
            }
        }
    }
}

int main() {
    std::cout << "Mock engine started.\n";

    using namespace simple_socket;

    init();

    int sock = create_client_socket("127.0.0.1", 4712);

    if (sock < 0) {
        std::cerr << "Failed to connect.\n";
        return 1;
    }

    // TS側へブレイクポイント要求メッセージを送信
    std::string req = "GET_BREAKPOINTS\n";
    send(sock, req.c_str(), req.size(), 0);

    // ブレイクポイント情報受信用のバッファ
    std::string response;
    char buf[1024] = {};
    // "END_BREAKPOINTS"が現れるまで受信
    while (true) {
        int len = recv(sock, buf, sizeof(buf) - 1, 0);
        if (len <= 0) {
            break;
        }
        buf[len] = '\0';
        response += buf;
        if (response.find("END_BREAKPOINTS") != std::string::npos)
            break;
    }

    // 受信したブレイクポイント情報をパース
    parseBreakpoints(response);

    // 受信結果の表示
    std::cout << "Received breakpoints:" << std::endl;
    for (const auto &entry : g_breakpoints) {
        const std::string &filepath = entry.first;
        for (const auto &bp : entry.second) {
            std::cout << "File: " << filepath << ", Line: " << bp.line
                      << std::endl;
        }
    }

    // ここ以降は既存の通信例（PING/HELLO等）でも可能
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Sending: PING\n";
    std::string ping = "PING\n";
    send(sock, ping.c_str(), ping.size(), 0);
    memset(buf, 0, sizeof(buf));
    int len = recv(sock, buf, sizeof(buf) - 1, 0);
    if (len > 0) {
        std::cout << "Server: " << std::string(buf, len);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Sending: HELLO\n";
    std::string hello = "HELLO\n";
    send(sock, hello.c_str(), hello.size(), 0);
    memset(buf, 0, sizeof(buf));
    len = recv(sock, buf, sizeof(buf) - 1, 0);
    if (len > 0) {
        std::cout << "Server: " << std::string(buf, len);
    }

    close_socket(sock);
    cleanup();
    return 0;
}
