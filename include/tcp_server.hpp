#pragma once

#include "aof_log.hpp"
#include "command_handler.hpp"
#include "command_parser.hpp"
#include "key_value_store.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class TcpServer {
public:
    explicit TcpServer(std::uint16_t port, const std::string& aof_path = "");

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    bool start();
    void run();
    void stop();

private:
    std::uint16_t port_;
    int listen_fd_;
    std::atomic_bool running_;
    KeyValueStore store_;
    CommandParser parser_;
    std::unique_ptr<AofLog> aof_log_;
    CommandHandler command_handler_;
    std::vector<std::thread> client_threads_;
    std::mutex client_threads_mutex_;

    bool setup_platform();
    void cleanup_platform();
    void load_aof();
    void start_client_thread(int client_fd);
    void join_client_threads();
    void handle_client(int client_fd);
    bool send_response(int client_fd, const std::string& response);
};
