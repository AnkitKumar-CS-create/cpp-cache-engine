#pragma once

#include "command.hpp"

#include <cstddef>
#include <string>

enum class ParseStatus {
    Complete,
    Incomplete,
    Error
};

struct ParseResult {
    ParseStatus status;
    Command command;
    std::size_t bytes_consumed;
    std::string error_message;
};

class CommandParser {
public:
    Command parse(const std::string& line) const;
    ParseResult try_parse(const std::string& buffer) const;

private:
    ParseResult parse_simple_line(const std::string& buffer) const;
    ParseResult parse_resp_array(const std::string& buffer) const;
};
