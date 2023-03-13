#include <gtest/gtest.h>
#include <string>
#include "parser.h"
#include "executor.h"

using std::string;

TEST(ExecutorTest, command) {
    string input = "cat /etc/passwd";
    CommandArray commands = parser(input.c_str());
    ASSERT_EQ(execute(commands), 1);
    parser_free(&commands);
}

TEST(ExecutorTest, commandAndCommand) {
    string input = "cat /etc/crontab && cat /etc/passwd";
    CommandArray commands = parser(input.c_str());
    ASSERT_EQ(execute(commands), 2);
    parser_free(&commands);
}

TEST(ExecutorTest, falseAndCommand) {
    string input = "false && cat /etc/passwd";
    CommandArray commands = parser(input.c_str());
    ASSERT_EQ(execute(commands), 1);
    parser_free(&commands);
}

TEST(ExecutorTest, trueOrCommand) {
    string input = "true || cat /etc/passwd";
    CommandArray commands = parser(input.c_str());
    ASSERT_EQ(execute(commands), 1);
    parser_free(&commands);
}

TEST(ExecutorTest, commandPipeCommand) {
    string input = "cat /etc/passwd | grep root";
    CommandArray commands = parser(input.c_str());
    ASSERT_EQ(execute(commands), 2);
    parser_free(&commands);
}

TEST(ExecutorTest, commandPipeCommandPipeCommand) {
    string input = "cat /etc/passwd|grep nologin|grep sys";
    CommandArray commands = parser(input.c_str());
    ASSERT_EQ(execute(commands), 3);
    parser_free(&commands);
}