#pragma once

#include "command.hpp"
#include "command_parser.hpp"

#include <mutex>
#include <string>
#include <vector>

class AofLog {
public:
    explicit AofLog(std::string path);

    bool append(const Command& command) const;
    std::vector<Command> load_commands(const CommandParser& parser) const;

private:
    std::string path_;
    mutable std::mutex mutex_;

    std::string serialize(const Command& command) const;
};
