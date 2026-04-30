#include "command_handler.hpp"

#include <cassert>
#include <chrono>
#include <thread>

void run_command_parser_tests();
void run_key_value_store_tests();
void run_aof_log_tests();

void test_handler_ping() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"PING", {}}) == "+PONG\r\n");
}

void test_handler_set_and_get() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"SET", {"name", "Slava"}}) == "+OK\r\n");
    assert(handler.handle(Command{"GET", {"name"}}) == "$5\r\nSlava\r\n");
}

void test_handler_get_missing_key() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"GET", {"missing"}}) == "$-1\r\n");
}

void test_handler_del_and_exists() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"SET", {"name", "Slava"}}) == "+OK\r\n");
    assert(handler.handle(Command{"EXISTS", {"name"}}) == ":1\r\n");
    assert(handler.handle(Command{"DEL", {"name"}}) == ":1\r\n");
    assert(handler.handle(Command{"EXISTS", {"name"}}) == ":0\r\n");
    assert(handler.handle(Command{"DEL", {"name"}}) == ":0\r\n");
}

void test_handler_rejects_wrong_argument_count() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"SET", {"name"}}) == "-ERR wrong number of arguments for 'SET'\r\n");
    assert(handler.handle(Command{"GET", {"name", "extra"}}) == "-ERR wrong number of arguments for 'GET'\r\n");
}

void test_handler_rejects_unknown_command() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"NOPE", {}}) == "-ERR unknown command\r\n");
}

void test_handler_incr_missing_key() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"INCR", {"counter"}}) == ":1\r\n");
    assert(handler.handle(Command{"GET", {"counter"}}) == "$1\r\n1\r\n");
}

void test_handler_incr_existing_integer() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"SET", {"counter", "9"}}) == "+OK\r\n");
    assert(handler.handle(Command{"INCR", {"counter"}}) == ":10\r\n");
}

void test_handler_incr_non_integer_error() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"SET", {"name", "Slava"}}) == "+OK\r\n");
    assert(handler.handle(Command{"INCR", {"name"}}) == "-ERR value is not an integer\r\n");
}

void test_handler_expire_existing_key() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"SET", {"name", "Slava"}}) == "+OK\r\n");
    assert(handler.handle(Command{"EXPIRE", {"name", "10"}}) == ":1\r\n");
}

void test_handler_expire_missing_key() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"EXPIRE", {"missing", "10"}}) == ":0\r\n");
}

void test_handler_expire_rejects_invalid_seconds() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"SET", {"name", "Slava"}}) == "+OK\r\n");
    assert(handler.handle(Command{"EXPIRE", {"name", "0"}}) == "-ERR invalid expire time\r\n");
    assert(handler.handle(Command{"EXPIRE", {"name", "-3"}}) == "-ERR invalid expire time\r\n");
    assert(handler.handle(Command{"EXPIRE", {"name", "soon"}}) == "-ERR invalid expire time\r\n");
}

void test_handler_ttl_missing_key() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"TTL", {"missing"}}) == ":-2\r\n");
}

void test_handler_ttl_without_expiration() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"SET", {"name", "Slava"}}) == "+OK\r\n");
    assert(handler.handle(Command{"TTL", {"name"}}) == ":-1\r\n");
}

void test_handler_expired_key_behaves_as_missing() {
    KeyValueStore store;
    CommandHandler handler(store);

    assert(handler.handle(Command{"SET", {"name", "Slava"}}) == "+OK\r\n");
    assert(handler.handle(Command{"EXPIRE", {"name", "1"}}) == ":1\r\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    assert(handler.handle(Command{"GET", {"name"}}) == "$-1\r\n");
    assert(handler.handle(Command{"EXISTS", {"name"}}) == ":0\r\n");
    assert(handler.handle(Command{"TTL", {"name"}}) == ":-2\r\n");
}

void run_command_handler_tests() {
    test_handler_ping();
    test_handler_set_and_get();
    test_handler_get_missing_key();
    test_handler_del_and_exists();
    test_handler_rejects_wrong_argument_count();
    test_handler_rejects_unknown_command();
    test_handler_incr_missing_key();
    test_handler_incr_existing_integer();
    test_handler_incr_non_integer_error();
    test_handler_expire_existing_key();
    test_handler_expire_missing_key();
    test_handler_expire_rejects_invalid_seconds();
    test_handler_ttl_missing_key();
    test_handler_ttl_without_expiration();
    test_handler_expired_key_behaves_as_missing();
}

int main() {
    run_aof_log_tests();
    run_command_parser_tests();
    run_key_value_store_tests();
    run_command_handler_tests();

    return 0;
}
