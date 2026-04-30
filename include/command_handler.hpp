#pragma once

#include "aof_log.hpp"
#include "command.hpp"
#include "key_value_store.hpp"

#include <cstddef>
#include <string>

class CommandHandler {
public:
    explicit CommandHandler(KeyValueStore& store, AofLog* aof_log = nullptr);

    std::string handle(const Command& command);
    std::string replay(const Command& command);

private:
    KeyValueStore& store_;
    AofLog* aof_log_;

    std::string execute(const Command& command, bool should_log);
    std::string require_arg_count(const Command& command, std::size_t expected) const;
    bool parse_positive_integer(const std::string& text, long long& value) const;
    void append_if_enabled(const Command& command, bool should_log) const;
};
