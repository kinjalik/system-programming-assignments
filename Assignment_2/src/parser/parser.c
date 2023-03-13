//
// Created by kinjalik on 3/10/23.
//
#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "parser.h"

#define INITIAL_SEQUENCE_SIZE 8
#define EXTENSION_SIZE 2

static CommandArray initArray();
static uint32_t extendCommandArray(CommandArray *array);
static Command *allocateCommand(CommandArray *array);

static CommandArray initArray() {
    size_t commandsSize = INITIAL_SEQUENCE_SIZE;
    Command *commands = (Command *) calloc(commandsSize, sizeof(Command));

    CommandArray ret = {
            .tokens = commands,
            .size = 0,
            .capacity = INITIAL_SEQUENCE_SIZE
    };
    return ret;
}

static uint32_t extendCommandArray(CommandArray *array) {
    size_t newCapacity = array->capacity + EXTENSION_SIZE;
    Command *newArray = (Command *) calloc(newCapacity, sizeof(Command));
    if (newArray == NULL) {
        printf("Failed to allocate extended command array\n");
        return EXIT_FAILURE;
    }
    memcpy(newArray, array->tokens, array->size * sizeof(Command));
    free(array->tokens);
    array->tokens = newArray;
    return EXIT_SUCCESS;
}

static Command *allocateCommand(CommandArray *array) {
    if (array->size == array->capacity) {
        extendCommandArray(array);
    }
    Command *ret = &array->tokens[array->size];
    ret->argv = (char **) calloc(INITIAL_SEQUENCE_SIZE, sizeof(char *));
    ret->argc = 0;
    ret->argCapacity = INITIAL_SEQUENCE_SIZE;

    array->size++;
    return ret;
}

static uint32_t extendArgv(Command *cmd) {
    size_t newCap = cmd->argCapacity;
    char **newArgv = (char **) malloc(newCap * sizeof(char *) + 1); // argv must be null-terminated array
    memset(newArgv, 0, newCap * sizeof(char *) + 1);
    if (newArgv == NULL) {
        return EXIT_FAILURE;
    }
    memcpy(newArgv, cmd->argv, sizeof(char *) * cmd->argc);
    free(cmd->argv);
    cmd->argCapacity = newCap;
    return EXIT_SUCCESS;
}

static void addArg(Command *cmd, const char *const arg, size_t len) {
    if (cmd->argc == cmd->argCapacity) {
        extendArgv(cmd);
    }

    size_t i = cmd->argc;
    cmd->argc++;
    cmd->argv[i] = (char *) calloc(len + 1, sizeof(char));
    memcpy(cmd->argv[i], arg, len);
    cmd->argv[i][len] = 0;
    if (i == 0) {
        cmd->name = cmd->argv[i];
    }
}

bool isSpecialChar(char ch) {
    switch (ch) {
        case '|':
        case '&':
        case '>':
            return true;
        default:
            return false;
    }
}

bool isSpecialToken(char *t) {
    return isSpecialChar(t[0]);
}

CommandArray parser(char **input) {
    CommandArray ca = initArray();

    char **argList = (char **) calloc(INITIAL_SEQUENCE_SIZE, sizeof(char *));
    size_t argCount = 0;
    size_t argCap = INITIAL_SEQUENCE_SIZE;

    size_t last_mirrored_slash = -1;
    do {
        char *buf = NULL;
        size_t bufLen = 0;
        long ret = getline(&buf, &bufLen, stdin);
        if (ret == -1) {
            exit(EXIT_FAILURE);
        }
        size_t len = ret;

        size_t startPos = 0;
        while (startPos < len) {
            bool singleParOpened = false;
            bool doubleParOpened = false;
            while (startPos < len && (
                    buf[startPos] == ' ' ||
                    buf[startPos] == '\n' && startPos == 0
                )) {
                startPos++;
            }

            if (startPos >= len || buf[startPos] == '\n')
                break;

            size_t endPos = startPos;

            if (isSpecialChar(buf[endPos])) {
                while (isSpecialChar(buf[endPos]))
                    endPos++;
                goto ADD_TO_ARG_LIST;
            }

            bool trimParen = false;
            while (endPos < len - 1) {
                if (buf[endPos] == '\\') {
                    if (buf[endPos + 1] == '\n') {
                        buf[endPos] = 0;
                        char *nextLine = NULL;
                        size_t newBufSize = 0;
                        long ret  = getline(&nextLine, &newBufSize, stdin);
                        if (ret == -1) {
                            exit(EXIT_FAILURE);
                        }
                        size_t newLen = ret;

                        char *newBuf = (char *) calloc(len + newLen + 1, sizeof(char));
                        strcat(newBuf, buf);
                        strcat(newBuf, nextLine);
                        free(nextLine);
                        free(buf);

                        buf = newBuf;
                        len += newLen - 1; // -2 for removed mirrored newline
                        endPos = startPos;
                        continue;
                    }

                    last_mirrored_slash = endPos + 1;
                    endPos += 2;
                    continue;
                }

//                if (buf[endPos] == ' ' && endPos > 1 && buf[endPos - 1] == '\\') {
//                    memmove(&buf[endPos - 1], &buf[endPos], len - endPos);
//                    len--;
//                }

                if ((buf[endPos] == ' ' || buf[endPos] == '\n' || isSpecialChar(buf[endPos]))
                    && !singleParOpened && !doubleParOpened) {
                    break;
                }

                if (buf[endPos] == '"') {
                    if (singleParOpened || endPos - 1 >= 0 && buf[endPos - 1] == '\\' && last_mirrored_slash != endPos - 1) {
                    } else {
                        doubleParOpened = !doubleParOpened;
                        trimParen = true;
                        if (!doubleParOpened)
                            break;
                    }
                }

                if (buf[endPos] == '\'') {
                    if (doubleParOpened || endPos - 1 >= 0 && buf[endPos - 1] == '\\' && last_mirrored_slash != endPos - 1) {
                    } else {
                        singleParOpened = !singleParOpened;
                        trimParen = true;
                        if (!singleParOpened)
                            break;
                    }
                }
                endPos++;
            }

            if (doubleParOpened || singleParOpened) {
                char *nextLine = NULL;
                size_t newBufSize = 0;
                long ret = getline(&nextLine, &newBufSize, stdin);
                if (ret == -1) {
                    exit(EXIT_FAILURE);
                }
                size_t newLen = ret;

                char *newBuf = (char *) calloc(len + newLen + 1, sizeof(char));
                strcat(newBuf, buf);
                strcat(newBuf, nextLine);
                free(nextLine);
                free(buf);

                buf = newBuf;
                len += newLen;
                continue;
            }

            ADD_TO_ARG_LIST:
            if (argCount == argCap) {
                argCap += EXTENSION_SIZE;
                argList = realloc(argList, argCap * sizeof(char *));
            }
            size_t argLen = !trimParen ? endPos - startPos : endPos - startPos - 1;
            argList[argCount] = (char *) calloc(argLen, sizeof(char));

//            memcpy(argList[argCount], , argLen);
            for (size_t i = 0, destI = 0; i < argLen; i++, destI++) {
                size_t idx = i + (trimParen ? startPos + 1 : startPos);
                char out = buf[idx];
                if (out == '\\') {
                    i++;
                    out = buf[idx + 1];
                }
                argList[argCount][destI] = out;
            }
//            printf("[%lu] %s\n", argCount, argList[argCount]);
            argCount++;
            startPos = trimParen ? endPos + 1 : endPos;
            trimParen = false;
        }
        free(buf);
        break;
    } while (0);

    for (size_t i = 0; i < argCount; i++) {
        if (isSpecialToken(argList[i])) {
            CommandType type = COMMAND_TYPE_UNKNOWN;
            if (argList[i][0] == '&' && argList[i][1] == '&') {
                type = COMMAND_TYPE_OPERATOR_AND;
            } else if (argList[i][0] == '|' && argList[i][1] != '|') {
                type = COMMAND_TYPE_OPERATOR_PIPE;
            } else if (argList[i][0] == '|' && argList[i][1] == '|') {
                type = COMMAND_TYPE_OPERATOR_OR;
            } else if (argList[i][0] == '>' && argList[i][1] != '>') {
                type = COMMAND_TYPE_OPERATOR_WRITE;
            } else if (argList[i][0] == '>' && argList[i][1] == '>') {
                type = COMMAND_TYPE_OPERATOR_APPEND;
            }
            Command *cmd = allocateCommand(&ca);
            cmd->type = type;
        } else {
            Command *cmd = allocateCommand(&ca);
            for (; i < argCount; i++) {
                if (isSpecialToken(argList[i]) ) {
                    i--;
                    break;
                }
                if (strlen(argList[i]) == 0) {
                    continue;
                }
                addArg(cmd, argList[i], strlen(argList[i]));
            }
            if (cmd->argv[0][0] == '#')
                cmd->type = COMMAND_TYPE_COMMENT;
        }
    }

    for (size_t i = 0; i < argCount; i++) {
        free(argList[i]);
    }
    free(argList);

    return ca;
}

void parser_free(CommandArray *const ptr) {
}
/*

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
size_t parseSpecial_test(const char *const input, const size_t length, size_t pos, Command *const cmd, CommandType type) {
    return parseSpecial(input, length, pos, cmd, type);
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
        for (size_t i = 0; i < cmd->argCapacity; i++) {
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
        // Need malloc to make sure array is null-terminated
        cmd->argv = (char **) malloc(INITIAL_SEQUENCE_SIZE * sizeof(char *) + 1);
        memset(cmd->argv, 0, INITIAL_SEQUENCE_SIZE * sizeof(char *) + 1);
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

static size_t parseRegular(const char *const input, const size_t length, size_t pos, CommandArray *ca) {
    assert(pos < length);
    if (ca->size == ca->capacity) {
        extendCommandArray(ca);
    }
    Command *cmd = &ca->tokens[ca->size];
    ca->size++;

    while (pos < length && !isSpecialCharacter(input[pos])) {
        if (input[pos] == ' ') {
            pos++;
            continue;
        }

        size_t startPos = pos;
        size_t endPos = pos;

        bool inParen = false;
        while (endPos < length) {
            if (input[endPos] == '"') {
                if (!inParen) {
                    startPos++;
                } else {
                    break;
                }
                inParen = !inParen;
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
        token[endPos - startPos] = '\0';

        if (ca->name == NULL) {
            ca->name = token;
        }
        insertToStringSequence(token, ca);

//        printf("startPos=%lu, endPos=%lu: %s\n", startPos, endPos, token);
        if (!inParen) {
            pos = endPos;
        } else {
            pos = endPos + 1;
        }
    }
    return pos;
}

static size_t parseSpecial(const char *const input, const size_t length, size_t pos, Command *const cmd, CommandType type) {
    assert(pos < length);
    while (pos < length && input[pos] == ' ') {
        pos++;
    }

    while (pos < length && isSpecialCharacter(input[pos])) {
        pos++;
    }

    cmd->type = type;
    return pos;
}
 */