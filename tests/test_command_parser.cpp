#include "command_parser.hpp"

#include <cassert>

void test_parser_normalizes_command_name() {
    CommandParser parser;

    Command command = parser.parse("set name Slava");

    assert(command.name == "SET");
    assert(command.args.size() == 2);
    assert(command.args[0] == "name");
    assert(command.args[1] == "Slava");
}

void test_parser_handles_extra_whitespace() {
    CommandParser parser;

    Command command = parser.parse("   GET    name   ");

    assert(command.name == "GET");
    assert(command.args.size() == 1);
    assert(command.args[0] == "name");
}

void test_parser_handles_empty_input() {
    CommandParser parser;

    Command command = parser.parse("   ");

    assert(command.name.empty());
    assert(command.args.empty());
}

void test_parser_try_parse_simple_line() {
    CommandParser parser;

    ParseResult result = parser.try_parse("GET name\r\n");

    assert(result.status == ParseStatus::Complete);
    assert(result.bytes_consumed == 10);
    assert(result.command.name == "GET");
    assert(result.command.args.size() == 1);
    assert(result.command.args[0] == "name");
}

void test_parser_resp_ping() {
    CommandParser parser;

    ParseResult result = parser.try_parse("*1\r\n$4\r\nPING\r\n");

    assert(result.status == ParseStatus::Complete);
    assert(result.command.name == "PING");
    assert(result.command.args.empty());
}

void test_parser_resp_set() {
    CommandParser parser;

    ParseResult result = parser.try_parse("*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$5\r\nSlava\r\n");

    assert(result.status == ParseStatus::Complete);
    assert(result.command.name == "SET");
    assert(result.command.args.size() == 2);
    assert(result.command.args[0] == "name");
    assert(result.command.args[1] == "Slava");
}

void test_parser_resp_get() {
    CommandParser parser;

    ParseResult result = parser.try_parse("*2\r\n$3\r\nGET\r\n$4\r\nname\r\n");

    assert(result.status == ParseStatus::Complete);
    assert(result.command.name == "GET");
    assert(result.command.args.size() == 1);
    assert(result.command.args[0] == "name");
}

void test_parser_resp_incomplete_input() {
    CommandParser parser;

    ParseResult result = parser.try_parse("*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$5\r\nSla");

    assert(result.status == ParseStatus::Incomplete);
}

void test_parser_resp_invalid_input() {
    CommandParser parser;

    ParseResult result = parser.try_parse("*2\r\n+PING\r\n$4\r\nname\r\n");

    assert(result.status == ParseStatus::Error);
    assert(result.error_message == "expected RESP bulk string");
}

void run_command_parser_tests() {
    test_parser_normalizes_command_name();
    test_parser_handles_extra_whitespace();
    test_parser_handles_empty_input();
    test_parser_try_parse_simple_line();
    test_parser_resp_ping();
    test_parser_resp_set();
    test_parser_resp_get();
    test_parser_resp_incomplete_input();
    test_parser_resp_invalid_input();
}
