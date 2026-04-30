#include "tcp_server.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace {

struct ServerConfig {
    std::uint16_t port = 6379;
    std::string aof_path;
};

ServerConfig parse_config(int argc, char* argv[]) {
    ServerConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string option = argv[i];

        if (option == "--port") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("missing value for --port");
            }

            int parsed_port = std::stoi(argv[++i]);
            if (parsed_port < 1 || parsed_port > 65535) {
                throw std::out_of_range("port must be between 1 and 65535");
            }

            config.port = static_cast<std::uint16_t>(parsed_port);
        } else if (option == "--aof") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("missing value for --aof");
            }

            config.aof_path = argv[++i];
        } else {
            throw std::invalid_argument("usage: miniredis [--port PORT] [--aof PATH]");
        }
    }

    return config;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        ServerConfig config = parse_config(argc, argv);
        TcpServer server(config.port, config.aof_path);

        if (!server.start()) {
            return 1;
        }

        server.run();
        server.stop();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
