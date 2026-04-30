#pragma once

#include <string>

class RespResponse {
public:
    static std::string simple_string(const std::string& value);
    static std::string error(const std::string& message);
    static std::string integer(long long value);
    static std::string bulk_string(const std::string& value);
    static std::string null_bulk_string();
};

