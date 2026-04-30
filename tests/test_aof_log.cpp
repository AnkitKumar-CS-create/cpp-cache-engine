#include "aof_log.hpp"
#include "command_handler.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

namespace {

std::string test_file_path(const std::string& name) {
    return name + ".tmp.aof";
}

void remove_file(const std::string& path) {
    std::remove(path.c_str());
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

} // namespace

void test_aof_appends_mutating_commands() {
    std::string path = test_file_path("mutating_commands");
    remove_file(path);

    AofLog aof(path);
    KeyValueStore store;
    CommandHandler handler(store, &aof);

    assert(handler.handle(Command{"SET", {"name", "Slava"}}) == "+OK\r\n");
    assert(handler.handle(Command{"INCR", {"count"}}) == ":1\r\n");
    assert(handler.handle(Command{"EXPIRE", {"name", "60"}}) == ":1\r\n");
    assert(handler.handle(Command{"DEL", {"name"}}) == ":1\r\n");

    assert(read_file(path) == "SET name Slava\nINCR count\nEXPIRE name 60\nDEL name\n");
    remove_file(path);
}

void test_aof_does_not_append_read_only_commands() {
    std::string path = test_file_path("read_only_commands");
    remove_file(path);

    AofLog aof(path);
    KeyValueStore store;
    CommandHandler handler(store, &aof);

    assert(handler.handle(Command{"PING", {}}) == "+PONG\r\n");
    assert(handler.handle(Command{"GET", {"name"}}) == "$-1\r\n");
    assert(handler.handle(Command{"EXISTS", {"name"}}) == ":0\r\n");
    assert(handler.handle(Command{"TTL", {"name"}}) == ":-2\r\n");

    assert(read_file(path).empty());
    remove_file(path);
}

void test_aof_loads_commands_from_file() {
    std::string path = test_file_path("load_commands");
    remove_file(path);

    {
        std::ofstream file(path);
        file << "SET name Slava\n";
        file << "INCR count\n";
        file << "GET ignored\n";
    }

    AofLog aof(path);
    CommandParser parser;
    std::vector<Command> commands = aof.load_commands(parser);

    assert(commands.size() == 3);
    assert(commands[0].name == "SET");
    assert(commands[0].args[0] == "name");
    assert(commands[0].args[1] == "Slava");
    assert(commands[1].name == "INCR");
    assert(commands[1].args[0] == "count");

    remove_file(path);
}

void test_aof_replay_restores_set_incr_del_behavior() {
    std::string path = test_file_path("replay_restore");
    remove_file(path);

    {
        std::ofstream file(path);
        file << "SET name Slava\n";
        file << "INCR count\n";
        file << "INCR count\n";
        file << "DEL name\n";
    }

    AofLog aof(path);
    CommandParser parser;
    KeyValueStore store;
    CommandHandler handler(store, &aof);

    for (const Command& command : aof.load_commands(parser)) {
        handler.replay(command);
    }

    assert(handler.handle(Command{"GET", {"name"}}) == "$-1\r\n");
    assert(handler.handle(Command{"GET", {"count"}}) == "$1\r\n2\r\n");

    remove_file(path);
}

void run_aof_log_tests() {
    test_aof_appends_mutating_commands();
    test_aof_does_not_append_read_only_commands();
    test_aof_loads_commands_from_file();
    test_aof_replay_restores_set_incr_del_behavior();
}
