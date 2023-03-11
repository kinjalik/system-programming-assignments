//
// Created by kinjalik on 3/10/23.
//
#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "parser.h"

#define INITIAL_SEQUENCE_SIZE 8
#define EXTENSION_SIZE 2

static CommandType decideCommandType(const char *input, size_t length, size_t pos);
static size_t parseRegular(const char *const input, const size_t length, size_t pos, Command *const cmd);
static void freeCommand(Command *const cmd);

CommandArray parser(const char *const input, size_t length) {
    Command *commands = (Command *) calloc(INITIAL_SEQUENCE_SIZE, sizeof(Command));

}

void parser_free(CommandArray *const ptr) {
    if (ptr == NULL) { return; }
    for (size_t i = 0; ptr->tokens != NULL && i < ptr->tokenCount; i++) {
        Command *token = &ptr->tokens[i];
        freeCommand(token);
    }
    free(ptr->tokens);
    ptr->tokenCount = 0;
    ptr->tokens = NULL;
}

#ifdef DEBUG
CommandType decideCommandType_test(const char *const input, size_t length, size_t pos) {
    return decideCommandType(input, length, pos);
}
size_t parseRegular_test(const char *const input, const size_t length, size_t pos, Command *const cmd) {
    return parseRegular(input, length, pos, cmd);
}
void freeCommand_test(Command *const cmd) {
    return freeCommand(cmd);
}
#endif

static CommandType decideCommandType(const char *const input, size_t length, size_t pos) {
    assert(pos < length);
    if (input[pos] == '|') {
        if (pos + 1 < length && input[pos + 1] == '|') {
            return COMMAND_TYPE_OPERATOR_OR;
        }
        return COMMAND_TYPE_OPERATOR_PIPE;
    }

    if (input[pos] == '&') {
        if (pos + 1 < length && input[pos + 1] == '&') {
            return COMMAND_TYPE_OPERATOR_AND;
        }
        return COMMAND_TYPE_BACKGROUND;
    }

    if (input[pos] == '>') {
        if (pos + 1 < length && input[pos + 1] == '>') {
            return COMMAND_TYPE_OPERATOR_APPEND;
        }
        return COMMAND_TYPE_OPERATOR_WRITE;
    }

    return COMMAND_TYPE_REGULAR;
}

static bool isSpecialCharacter(const char ch) {
    switch (ch) {
        case '|':
        case '&':
        case '>':
            return true;
    }
    return false;
}

static void freeCommand(Command *const cmd) {
    if (cmd == NULL) {
        return;
    }

    if (cmd->argv != NULL) {
        for (size_t i = 0; i < cmd->argc; i++) {
            free(cmd->argv[i]);
            cmd->argv[i] = 0;
        }
        cmd->argv = NULL;
        cmd->argc = 0;
        cmd->argCapacity = 0;
    }
    free(cmd->argv);
}

static void insertToStringSequence(char *token, Command *const cmd) {
    if (cmd->argv == NULL) {
        cmd->argv = (char **) calloc(INITIAL_SEQUENCE_SIZE, sizeof(char *));
        if (cmd->argv == NULL) {
            printf("Failed to allocate argv");
            return;
        }
        cmd->argCapacity = INITIAL_SEQUENCE_SIZE;
        cmd->argc = 0;
    }

    if (cmd->argc == cmd->argCapacity) {
        char **newArgv = (char **) calloc(cmd->argCapacity + EXTENSION_SIZE, sizeof(char *));
        if (newArgv == NULL) {
            printf("Failed to allocate argv for extension");
            return;
        }
        memcpy(newArgv, cmd->argv, sizeof(char *) * cmd->argc);

        free(cmd->argv);
        cmd->argv = newArgv;
        cmd->argCapacity = cmd->argCapacity + EXTENSION_SIZE;
    }

    cmd->argv[cmd->argc] = token;
    cmd->argc++;
}

static size_t parseRegular(const char *const input, const size_t length, size_t pos, Command *const cmd) {
    assert(pos < length);

    while (pos < length && !isSpecialCharacter(input[pos])) {
        if (input[pos] == ' ') {
            pos++;
            continue;
        }

        size_t startPos = pos;
        size_t endPos = pos;

        bool inParen = false;
        while (endPos < length) {
            if (inParen && input[endPos] == '"') {
                inParen = false;
            } else if (!inParen && input[endPos] == '"') {
                inParen = true;
            }

            if (!inParen && (isSpecialCharacter(input[endPos]) || input[endPos] == ' ')) {
                break;
            }
            endPos++;
        }

        char *token = (char *) calloc(endPos - startPos + 1, sizeof(char));
        if (token == NULL) {
            printf("Failed to allocate memory for token");
            return 0;
        }
        memcpy(token,
               input + sizeof(char) * startPos,
               endPos - startPos);
        token[endPos] = 0;

        if (cmd->name == NULL) {
            cmd->name = token;
        } else {
            insertToStringSequence(token, cmd);
        }

        printf("startPos=%lu, endPos=%lu: %s\n", startPos, endPos, token);
        pos = endPos;
    }
    return pos;

}