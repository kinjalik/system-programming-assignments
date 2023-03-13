#include <gtest/gtest.h>
#include <string>
#include "parser.h"

using std::string;

TEST(ParserTest, regular_and_regular) {
    string input = "echo \"TEST TEST TEST\" && cat test";

    CommandArray commands = parser(input.c_str());
    ASSERT_EQ(commands.tokenCount, 3);

    Command *echoToken = &commands.tokens[0];
    ASSERT_EQ(echoToken->type, COMMAND_TYPE_REGULAR);
    ASSERT_EQ(echoToken->argc, 2);
    ASSERT_STREQ(echoToken->name, "echo");

    Command *andToken = &commands.tokens[1];
    ASSERT_EQ(andToken->type, COMMAND_TYPE_OPERATOR_AND);

    Command *catToken = &commands.tokens[2];
    ASSERT_EQ(catToken->type, COMMAND_TYPE_REGULAR);
    ASSERT_STREQ(catToken->name, "cat");
}