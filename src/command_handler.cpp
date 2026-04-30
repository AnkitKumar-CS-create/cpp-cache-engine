#include "command_handler.hpp"

#include "resp_response.hpp"

#include <chrono>
#include <cstddef>
#include <string>

CommandHandler::CommandHandler(KeyValueStore& store, AofLog* aof_log)
    : store_(store),
      aof_log_(aof_log) {
}

std::string CommandHandler::handle(const Command& command) {
    return execute(command, true);
}

std::string CommandHandler::replay(const Command& command) {
    return execute(command, false);
}

std::string CommandHandler::execute(const Command& command, bool should_log) {
    if (command.name.empty()) {
        return RespResponse::error("empty command");
    }

    if (command.name == "PING") {
        std::string error = require_arg_count(command, 0);
        if (!error.empty()) {
            return error;
        }

        return RespResponse::simple_string("PONG");
    }

    if (command.name == "SET") {
        std::string error = require_arg_count(command, 2);
        if (!error.empty()) {
            return error;
        }

        store_.set(command.args[0], command.args[1]);
        append_if_enabled(command, should_log);
        return RespResponse::simple_string("OK");
    }

    if (command.name == "GET") {
        std::string error = require_arg_count(command, 1);
        if (!error.empty()) {
            return error;
        }

        auto value = store_.get(command.args[0]);
        if (!value.has_value()) {
            return RespResponse::null_bulk_string();
        }

        return RespResponse::bulk_string(*value);
    }

    if (command.name == "DEL") {
        std::string error = require_arg_count(command, 1);
        if (!error.empty()) {
            return error;
        }

        bool deleted = store_.del(command.args[0]);
        if (deleted) {
            append_if_enabled(command, should_log);
        }

        return RespResponse::integer(deleted ? 1 : 0);
    }

    if (command.name == "EXISTS") {
        std::string error = require_arg_count(command, 1);
        if (!error.empty()) {
            return error;
        }

        return RespResponse::integer(store_.exists(command.args[0]) ? 1 : 0);
    }

    if (command.name == "INCR") {
        std::string error = require_arg_count(command, 1);
        if (!error.empty()) {
            return error;
        }

        long long result = 0;
        if (!store_.incr(command.args[0], result)) {
            return RespResponse::error("value is not an integer");
        }

        append_if_enabled(command, should_log);
        return RespResponse::integer(result);
    }

    if (command.name == "EXPIRE") {
        std::string error = require_arg_count(command, 2);
        if (!error.empty()) {
            return error;
        }

        long long seconds = 0;
        if (!parse_positive_integer(command.args[1], seconds)) {
            return RespResponse::error("invalid expire time");
        }

        bool updated = store_.expire(command.args[0], std::chrono::seconds(seconds));
        if (updated) {
            append_if_enabled(command, should_log);
        }

        return RespResponse::integer(updated ? 1 : 0);
    }

    if (command.name == "TTL") {
        std::string error = require_arg_count(command, 1);
        if (!error.empty()) {
            return error;
        }

        return RespResponse::integer(store_.ttl(command.args[0]));
    }

    return RespResponse::error("unknown command");
}

std::string CommandHandler::require_arg_count(const Command& command, std::size_t expected) const {
    if (command.args.size() == expected) {
        return "";
    }

    return RespResponse::error("wrong number of arguments for '" + command.name + "'");
}

bool CommandHandler::parse_positive_integer(const std::string& text, long long& value) const {
    if (text.empty()) {
        return false;
    }

    std::size_t parsed_chars = 0;

    try {
        value = std::stoll(text, &parsed_chars);
    } catch (...) {
        return false;
    }

    return parsed_chars == text.size() && value > 0;
}

void CommandHandler::append_if_enabled(const Command& command, bool should_log) const {
    if (should_log && aof_log_ != nullptr) {
        aof_log_->append(command);
    }
}
