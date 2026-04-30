#include "tcp_server.hpp"

#include "resp_response.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <mutex>
#include <utility>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace {

constexpr int invalid_socket_value = -1;
constexpr int receive_buffer_size = 1024;

void close_socket(int fd) {
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(fd));
#else
    close(fd);
#endif
}

std::string last_socket_error() {
#ifdef _WIN32
    return "socket error code " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

} // namespace

TcpServer::TcpServer(std::uint16_t port, const std::string& aof_path)
    : port_(port),
      listen_fd_(invalid_socket_value),
      running_(false),
      store_(),
      parser_(),
      aof_log_(aof_path.empty() ? nullptr : std::make_unique<AofLog>(aof_path)),
      command_handler_(store_, aof_log_.get()) {
}

bool TcpServer::start() {
    if (!setup_platform()) {
        return false;
    }

    load_aof();

    listen_fd_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (listen_fd_ == invalid_socket_value) {
        std::cerr << "Failed to create socket: " << last_socket_error() << '\n';
        return false;
    }

    int reuse = 1;
    if (setsockopt(
            listen_fd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuse),
            sizeof(reuse)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR: " << last_socket_error() << '\n';
        close_socket(listen_fd_);
        listen_fd_ = invalid_socket_value;
        return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port " << port_ << ": " << last_socket_error() << '\n';
        close_socket(listen_fd_);
        listen_fd_ = invalid_socket_value;
        return false;
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        std::cerr << "Failed to listen: " << last_socket_error() << '\n';
        close_socket(listen_fd_);
        listen_fd_ = invalid_socket_value;
        return false;
    }

    running_ = true;
    std::cout << "MiniRedis listening on port " << port_ << '\n';
    return true;
}

void TcpServer::run() {
    while (running_) {
        sockaddr_in client_address {};
        socklen_t client_address_size = sizeof(client_address);

        int client_fd = static_cast<int>(accept(
            listen_fd_,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_address_size));

        if (client_fd == invalid_socket_value) {
            if (!running_) {
                break;
            }

            std::cerr << "Failed to accept client: " << last_socket_error() << '\n';
            continue;
        }

        std::cout << "Client connected\n";
        start_client_thread(client_fd);
    }
}

void TcpServer::stop() {
    running_ = false;

    if (listen_fd_ != invalid_socket_value) {
        close_socket(listen_fd_);
        listen_fd_ = invalid_socket_value;
    }

    join_client_threads();
    cleanup_platform();
}

bool TcpServer::setup_platform() {
#ifdef _WIN32
    WSADATA data {};
    int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << '\n';
        return false;
    }
#endif
    return true;
}

void TcpServer::cleanup_platform() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void TcpServer::load_aof() {
    if (aof_log_ == nullptr) {
        return;
    }

    std::vector<Command> commands = aof_log_->load_commands(parser_);
    for (const Command& command : commands) {
        // Replay rebuilds memory without appending the same command back to AOF.
        command_handler_.replay(command);
    }

    std::cout << "Loaded " << commands.size() << " command(s) from append-only file\n";
}

void TcpServer::start_client_thread(int client_fd) {
    std::thread client_thread([this, client_fd]() {
        handle_client(client_fd);
        close_socket(client_fd);
        std::cout << "Client disconnected\n";
    });

    std::scoped_lock lock(client_threads_mutex_);
    client_threads_.push_back(std::move(client_thread));
}

void TcpServer::join_client_threads() {
    std::vector<std::thread> threads_to_join;

    {
        std::scoped_lock lock(client_threads_mutex_);
        // Move the threads out while locked, then join without holding the mutex.
        threads_to_join.swap(client_threads_);
    }

    for (std::thread& thread : threads_to_join) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void TcpServer::handle_client(int client_fd) {
    std::string pending;
    char buffer[receive_buffer_size];

    while (true) {
        int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes_received <= 0) {
            break;
        }

        pending.append(buffer, bytes_received);

        while (!pending.empty()) {
            ParseResult result = parser_.try_parse(pending);

            if (result.status == ParseStatus::Incomplete) {
                break;
            }

            std::string response;
            if (result.status == ParseStatus::Error) {
                response = RespResponse::error(result.error_message);
            } else {
                response = command_handler_.handle(result.command);
            }

            if (!send_response(client_fd, response)) {
                return;
            }

            std::size_t bytes_consumed = result.bytes_consumed;
            if (bytes_consumed == 0 || bytes_consumed > pending.size()) {
                pending.clear();
                break;
            }

            pending.erase(0, bytes_consumed);
        }
    }
}

bool TcpServer::send_response(int client_fd, const std::string& response) {
    std::size_t total_sent = 0;

    while (total_sent < response.size()) {
        int sent = send(
            client_fd,
            response.c_str() + total_sent,
            static_cast<int>(response.size() - total_sent),
            0);

        if (sent <= 0) {
            std::cerr << "Failed to send response: " << last_socket_error() << '\n';
            return false;
        }

        total_sent += static_cast<std::size_t>(sent);
    }

    return true;
}
