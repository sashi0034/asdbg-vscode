#ifndef ASDBG_BACKEND_H
#define ASDBG_BACKEND_H

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
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

namespace asdbg {
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

// -----------------------------------------------

struct Breakpoint {
    std::string filename;
    int line;
};

class AsdbgBackend {
  public:
    AsdbgBackend(std::atomic<bool> &running) {
        simple_socket::init();

        const int sock = simple_socket::create_client_socket("127.0.0.1", 4712);
        if (sock < 0) {
            std::cerr << "Failed to connect to debugger.\n";
            throw std::runtime_error("Failed to connect to debugger.");
        }

        startReceiverThread(sock, running);

        reqestBreakpoints(sock);
    }

    ~AsdbgBackend() { simple_socket::cleanup(); }

  private:
    std::mutex m_breankpointMutex{};
    std::vector<Breakpoint> m_breankpointList{};

    void reqestBreakpoints(int sock) {
        std::string req = "GET_BREAKPOINTS\n";
        send(sock, req.c_str(), req.size(), 0);
    }

    void startReceiverThread(int sock, std::atomic<bool> &running) {
        std::thread([this, sock, &running]() {
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

                std::cout << "Received:\n" << buffer << "\n";

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
                std::string filepath, lineStr;
                if (std::getline(lineStream, filepath, ',') &&
                    std::getline(lineStream, lineStr)) {

                    try {
                        int lineNum = std::stoi(lineStr);
                        std::lock_guard<std::mutex> lock{m_breankpointMutex};
                        m_breankpointList.push_back(
                            Breakpoint{filepath, lineNum});

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
};

} // namespace asdbg

#endif // ASDBG_BACKEND_H
