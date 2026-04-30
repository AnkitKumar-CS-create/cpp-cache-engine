#include "aof_log.hpp"

#include <fstream>
#include <mutex>
#include <sstream>

AofLog::AofLog(std::string path)
    : path_(std::move(path)) {
}

bool AofLog::append(const Command& command) const {
    std::scoped_lock lock(mutex_);

    std::ofstream file(path_, std::ios::app);
    if (!file) {
        return false;
    }

    file << serialize(command) << '\n';
    return static_cast<bool>(file);
}

std::vector<Command> AofLog::load_commands(const CommandParser& parser) const {
    std::scoped_lock lock(mutex_);

    std::vector<Command> commands;
    std::ifstream file(path_);

    if (!file) {
        return commands;
    }

    std::string line;
    while (std::getline(file, line)) {
        Command command = parser.parse(line);
        if (!command.name.empty()) {
            commands.push_back(command);
        }
    }

    return commands;
}

std::string AofLog::serialize(const Command& command) const {
    std::ostringstream stream;
    stream << command.name;

    for (const std::string& arg : command.args) {
        stream << ' ' << arg;
    }

    return stream.str();
}
