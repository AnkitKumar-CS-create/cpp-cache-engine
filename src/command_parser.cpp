#include "command_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace {

std::string normalize_command_name(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
    return name;
}

std::string trim_line_end(std::string line) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    return line;
}

bool parse_non_negative_integer(const std::string& text, long long& value) {
    if (text.empty()) {
        return false;
    }

    std::size_t parsed_chars = 0;

    try {
        value = std::stoll(text, &parsed_chars);
    } catch (...) {
        return false;
    }

    return parsed_chars == text.size() && value >= 0;
}

ParseResult complete(Command command, std::size_t bytes_consumed) {
    return ParseResult{ParseStatus::Complete, command, bytes_consumed, ""};
}

ParseResult incomplete() {
    return ParseResult{ParseStatus::Incomplete, Command{}, 0, ""};
}

ParseResult error(const std::string& message, std::size_t bytes_consumed) {
    return ParseResult{ParseStatus::Error, Command{}, bytes_consumed, message};
}

} // namespace

Command CommandParser::parse(const std::string& line) const {
    std::istringstream stream(line);

    Command command;
    stream >> command.name;

    if (command.name.empty()) {
        return command;
    }

    command.name = normalize_command_name(command.name);

    std::string arg;
    while (stream >> arg) {
        command.args.push_back(arg);
    }

    return command;
}

ParseResult CommandParser::try_parse(const std::string& buffer) const {
    if (buffer.empty()) {
        return incomplete();
    }

    if (buffer[0] == '*') {
        return parse_resp_array(buffer);
    }

    return parse_simple_line(buffer);
}

ParseResult CommandParser::parse_simple_line(const std::string& buffer) const {
    std::size_t newline_position = buffer.find('\n');
    if (newline_position == std::string::npos) {
        return incomplete();
    }

    std::string line = buffer.substr(0, newline_position + 1);
    return complete(parse(trim_line_end(line)), newline_position + 1);
}

ParseResult CommandParser::parse_resp_array(const std::string& buffer) const {
    std::size_t cursor = 0;

    // RESP arrays are length-prefixed, so the parser can tell the TCP layer
    // whether it has a complete command or must wait for more bytes.
    std::size_t array_line_end = buffer.find("\r\n", cursor);
    if (array_line_end == std::string::npos) {
        return incomplete();
    }

    long long element_count = 0;
    if (!parse_non_negative_integer(buffer.substr(1, array_line_end - 1), element_count)) {
        return error("invalid RESP array length", array_line_end + 2);
    }

    if (element_count == 0) {
        return error("empty RESP array", array_line_end + 2);
    }

    cursor = array_line_end + 2;
    std::vector<std::string> parts;
    parts.reserve(static_cast<std::size_t>(element_count));

    for (long long i = 0; i < element_count; ++i) {
        if (cursor >= buffer.size()) {
            return incomplete();
        }

        if (buffer[cursor] != '$') {
            return error("expected RESP bulk string", cursor + 1);
        }

        std::size_t bulk_line_end = buffer.find("\r\n", cursor);
        if (bulk_line_end == std::string::npos) {
            return incomplete();
        }

        long long bulk_length = 0;
        std::string length_text = buffer.substr(cursor + 1, bulk_line_end - cursor - 1);
        if (!parse_non_negative_integer(length_text, bulk_length)) {
            return error("invalid RESP bulk string length", bulk_line_end + 2);
        }

        cursor = bulk_line_end + 2;

        std::size_t bytes_needed = static_cast<std::size_t>(bulk_length) + 2;
        if (buffer.size() - cursor < bytes_needed) {
            return incomplete();
        }

        if (buffer[cursor + static_cast<std::size_t>(bulk_length)] != '\r' ||
            buffer[cursor + static_cast<std::size_t>(bulk_length) + 1] != '\n') {
            return error("RESP bulk string missing CRLF terminator", cursor + bytes_needed);
        }

        parts.push_back(buffer.substr(cursor, static_cast<std::size_t>(bulk_length)));
        cursor += bytes_needed;
    }

    Command command;
    command.name = normalize_command_name(parts[0]);
    for (std::size_t i = 1; i < parts.size(); ++i) {
        command.args.push_back(parts[i]);
    }

    return complete(command, cursor);
}
