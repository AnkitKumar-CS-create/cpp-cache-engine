#include "resp_response.hpp"

std::string RespResponse::simple_string(const std::string& value) {
    return "+" + value + "\r\n";
}

std::string RespResponse::error(const std::string& message) {
    return "-ERR " + message + "\r\n";
}

std::string RespResponse::integer(long long value) {
    return ":" + std::to_string(value) + "\r\n";
}

std::string RespResponse::bulk_string(const std::string& value) {
    return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
}

std::string RespResponse::null_bulk_string() {
    return "$-1\r\n";
}

