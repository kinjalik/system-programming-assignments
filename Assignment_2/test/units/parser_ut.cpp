#include <gtest/gtest.h>
#include <string>

#include "parser.h"

using std::string;
extern "C" {
    CommandType decideCommandType_test(const char *input, size_t length, size_t pos);
    size_t parseRegular_test(const char *input, const size_t length, size_t pos, Command *const cmd);
    void freeCommand_test(Command *cmd);
}

TEST(ParserUnit, decideCommandType_regular) {
    string input = "echo \"TEST TEST TEST\"";

    CommandType result = decideCommandType_test(input.c_str(), input.length(), 0);

    ASSERT_EQ(result, COMMAND_TYPE_REGULAR) << "Incorrect type received: " << result;
}

TEST(ParserUnit, parseRegular_withParen) {
    string echoToken = "echo";
    string stringToken = "\"TEST TEST && TEST\"";
    string input = echoToken + " " + stringToken;
    Command cmd = {0};

    size_t newPos = parseRegular_test(input.c_str(), input.length(), 0, &cmd);
    ASSERT_EQ(newPos, input.size());
    ASSERT_STREQ(cmd.name, echoToken.c_str());
    ASSERT_EQ(cmd.argc, 1);
    ASSERT_STREQ(cmd.argv[0], stringToken.c_str());

    freeCommand_test(&cmd);
}

TEST(ParserUnit, parseRegular_decideAfterParseRegular) {
    string input = "echo \"SALAM ALEYKUM\" && echo \"Next!\"";
    Command cmd = {0};

    size_t newPos = parseRegular_test(input.c_str(), input.length(), 0, &cmd);
    CommandType nextCommandType = decideCommandType_test(input.c_str(), input.length(), newPos);
    ASSERT_EQ(nextCommandType, COMMAND_TYPE_OPERATOR_AND);
}