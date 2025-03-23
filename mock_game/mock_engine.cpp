#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
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

struct Breakpoint {
    std::string filepath;
    int line;
};

std::map<std::string, std::vector<Breakpoint>> g_breakpointMap;
std::mutex g_breakpointMutex;

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
            std::istringstream lineStream(line);
            std::string filepath, lineStr, colStr;
            if (std::getline(lineStream, filepath, ',') &&
                std::getline(lineStream, lineStr, ',') &&
                std::getline(lineStream, colStr)) {

                try {
                    int lineNum = std::stoi(lineStr);
                    std::lock_guard<std::mutex> lock(g_breakpointMutex);
                    g_breakpointMap[filepath].push_back({
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

void startReceiverThread(int sock, std::atomic<bool> &running) {
    std::thread([sock, &running]() {
        std::string buffer;
        char tempBuf[1024];

        while (running) {
            int len = recv(sock, tempBuf, sizeof(tempBuf) - 1, 0);
            if (len <= 0) {
                std::cerr << "Disconnected or error.\n";
                running = false;
                break;
            }

            tempBuf[len] = '\0';
            buffer += tempBuf;

            // 1行ずつ処理
            size_t pos;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);

                std::cout << "Server: " << line << std::endl;

                if (line == "BREAKPOINTS") {
                    std::string bpSection = line + "\n";
                    // collect until END_BREAKPOINTS
                    while ((pos = buffer.find("END_BREAKPOINTS")) ==
                           std::string::npos) {
                        len = recv(sock, tempBuf, sizeof(tempBuf) - 1, 0);
                        if (len <= 0)
                            break;
                        tempBuf[len] = '\0';
                        buffer += tempBuf;
                    }
                    pos = buffer.find("END_BREAKPOINTS");
                    if (pos != std::string::npos) {
                        size_t endPos = buffer.find('\n', pos);
                        bpSection += buffer.substr(0, endPos + 1);
                        parseBreakpoints(bpSection);
                        buffer.erase(0, endPos + 1);
                    }
                }
            }
        }
    }).detach();
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

    std::atomic<bool> running{true};
    startReceiverThread(sock, running);

    // ブレークポイント取得要求
    std::string req = "GET_BREAKPOINTS\n";
    send(sock, req.c_str(), req.size(), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "Sending: PING\n";
    std::string ping = "PING\n";
    send(sock, ping.c_str(), ping.size(), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "Sending: HELLO\n";
    std::string hello = "HELLO\n";
    send(sock, hello.c_str(), hello.size(), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 終了処理
    running = false;
    close_socket(sock);
    cleanup();

    return 0;
}
