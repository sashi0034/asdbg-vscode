#ifndef ASDBG_BACKEND_H
#define ASDBG_BACKEND_H

#include <cstring>
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

inline int create_socket(const std::string &ip, int port) {
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

inline int close_socket(int sock) {
#ifdef _WIN32
    return closesocket(sock);
#else
    return close(sock);
#endif
}

inline int send_data(int sock, const char *data, size_t size) {
    return send(sock, data, size, 0);
}

inline int receive_data(int sock, char *buffer, size_t buffer_size) {
    return recv(sock, buffer, buffer_size, 0);
}

} // namespace simple_socket

// -----------------------------------------------

#if __cplusplus >= 201703L
#include <string_view>
using string_view = std::string_view;
#else
class string_view {
  public:
    using size_type = std::size_t;
    using value_type = char;
    using const_iterator = const char *;

    string_view() noexcept : m_data(nullptr), m_size(0) {}

    string_view(const char *str)
        : m_data(str), m_size(str ? std::strlen(str) : 0) {}

    string_view(const std::string &str)
        : m_data(str.data()), m_size(str.size()) {}

    string_view(const char *str, size_type len) : m_data(str), m_size(len) {}

    const char *data() const noexcept { return m_data; }

    size_type size() const noexcept { return m_size; }

    bool empty() const noexcept { return m_size == 0; }

    const char &operator[](size_type pos) const { return m_data[pos]; }

    const_iterator begin() const noexcept { return m_data; }

    const_iterator end() const noexcept { return m_data + m_size; }

    string_view substr(size_type pos, size_type count = npos) const {
        if (pos > m_size)
            throw std::out_of_range("string_view::substr out of range");

        const auto len = count < m_size - pos ? count : m_size - pos;
        return string_view(m_data + pos, len);
    }

    // Since C++20
    // bool starts_with(const string_view &prefix) const {
    //     if (prefix.size() > size())
    //         return false;

    //     return std::memcmp(data(), prefix.data(), prefix.size()) == 0;
    // }

    // bool ends_with(const string_view &suffix) const {
    //     if (suffix.size() > size())
    //         return false;

    //     return std::memcmp(data() + size() - suffix.size(), suffix.data(),
    //                        suffix.size()) == 0;
    // }

    // Since C++17
    size_type find(char ch, size_type pos = 0) const {
        if (pos >= m_size)
            return npos;

        for (size_type i = pos; i < m_size; ++i) {
            if (m_data[i] == ch)
                return i;
        }

        return npos;
    }

    friend bool operator==(const string_view &lhs, const string_view &rhs) {
        if (lhs.m_size != rhs.m_size)
            return false;

        return std::memcmp(lhs.m_data, rhs.m_data, lhs.m_size) == 0;
    }

    friend bool operator!=(const string_view &lhs, const string_view &rhs) {
        return !(lhs == rhs);
    }

    friend bool operator==(const string_view &lhs, const char *rhs) {
        const size_type len = std::strlen(rhs);
        if (lhs.m_size != len)
            return false;

        return std::memcmp(lhs.m_data, rhs, len) == 0;
    }

    friend bool operator!=(const string_view &lhs, const char *rhs) {
        return !(lhs == rhs);
    }

    static const size_type npos = size_type(-1);

  private:
    const char *m_data;
    size_type m_size;
};
#endif

namespace detail {

std::vector<string_view> SplitLines(string_view data) {
    std::vector<string_view> lines;
    size_t pos;

    for (size_t pos = 0; pos < data.size();) {
        size_t endPos = data.find('\n', pos);
        if (endPos == string_view::npos) {
            endPos = data.size();
        }

        lines.push_back(data.substr(pos, endPos - pos));
        pos = endPos + 1;
    }

    return lines;
}

bool EndWith(string_view str, string_view suffix) {
    if (suffix.size() > str.size())
        return false;

    return std::memcmp(str.data() + str.size() - suffix.size(), suffix.data(),
                       suffix.size()) == 0;
}

class MessageQueue {
  public:
    MessageQueue(string_view data) : m_pos(0) { m_lines = SplitLines(data); }

    bool IsEmpty() const { return m_pos >= m_lines.size(); }

    string_view Peek() const {
        if (IsEmpty())
            return {};

        return m_lines[m_pos];
    }

    string_view Pop() {
        if (IsEmpty())
            return {};

        const auto pos = m_pos;
        m_pos++;
        return m_lines[pos];
    }

  private:
    std::vector<string_view> m_lines;
    size_t m_pos;
};
} // namespace detail

// -----------------------------------------------

struct Breakpoint {
    std::string filepath;
    int line;
};

class AsdbgBackend {
  public:
    AsdbgBackend() = default;

    void Start(std::atomic<bool> &running) {
        simple_socket::init();

        m_socket = simple_socket::create_socket("127.0.0.1", 4712);
        if (m_socket < 0) {
            std::cerr << "Failed to connect to debugger.\n";
            throw std::runtime_error("Failed to connect to debugger.");
        }

        StartReceiverThread(running);

        ReqestBreakpoints();
    }

    void Shutdown() {
        if (m_socket >= 0) {
            simple_socket::close_socket(m_socket);
            simple_socket::cleanup();
        }
    }

    bool IsBreakpoint(const std::string &filename, int line) {
        std::lock_guard<std::mutex> lock{m_breankpointMutex};

        for (const auto &bp : m_breankpointList) {
            if (bp.line == line &&
                detail::EndWith(bp.filepath,
                                filename) // FIXME: Consider relative paths
            ) {
                return true;
            }
        }

        return false;
    }

    ~AsdbgBackend() { Shutdown(); }

  private:
    int m_socket{-1};
    std::mutex m_breankpointMutex{};
    std::vector<Breakpoint> m_breankpointList{};

    void ReqestBreakpoints() {
        std::string request = "GET_BREAKPOINTS\n";
        simple_socket::send_data(m_socket, request.c_str(), request.size());
    }

    void StartReceiverThread(std::atomic<bool> &running) {
        std::thread([this, &running]() {
            char tmpBuffer[1024];

            while (running) {
                const int len = simple_socket::receive_data(
                    m_socket, tmpBuffer, sizeof(tmpBuffer) - 1);
                if (len <= 0) {
                    std::cerr << "Disconnected or error.\n";
                    running = false;
                    break;
                }

                tmpBuffer[len] = '\0';

                string_view bufferView = tmpBuffer;
                std::cout << "Received:\n" << bufferView.data() << "\n";
                auto messageQueue = detail::MessageQueue{bufferView};

                while (!messageQueue.IsEmpty()) {
                    if (ParseBeakpoints(messageQueue)) {
                        continue;
                    } else {
                        std::cout
                            << "Unknown message: " << messageQueue.Pop().data()
                            << std::endl;
                    }
                }
            }
        }).detach();
    }

    bool ParseBeakpoints(detail::MessageQueue &queue) {
        while (!queue.IsEmpty()) {
            if (queue.Peek() != "BREAKPOINTS")
                return false;

            queue.Pop();

            while (!queue.IsEmpty()) {
                const auto next = queue.Pop();
                if (next == "END_BREAKPOINTS") {
                    break;
                }

                std::istringstream lineStream(next.data());
                std::string filepath, line;

                try {

                    if (!std::getline(lineStream, filepath, ','))
                        throw std::runtime_error("Failed to parse filepath.");

                    if (!std::getline(lineStream, line))
                        throw std::runtime_error(
                            "Failed to parse line number.");

                    const int lineNumber = std::stoi(line);

                    std::lock_guard<std::mutex> lock{m_breankpointMutex};

                    m_breankpointList.push_back(
                        Breakpoint{filepath, lineNumber});

                    std::cout << "Parsed breakpoint: " << filepath << ", "
                              << lineNumber << std::endl;
                } catch (...) {
                    std::cerr << "Failed to parse breakpoint: " << line
                              << std::endl;
                }
            }

            break;
        }

        return true;
    }
};

} // namespace asdbg

#endif // ASDBG_BACKEND_H
