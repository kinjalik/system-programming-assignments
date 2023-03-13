//
// Created by kinjalik on 3/10/23.
//

#pragma once
#include <stdint.h>

typedef enum {
    COMMAND_TYPE_REGULAR,
    COMMAND_TYPE_OPERATOR_AND,
    COMMAND_TYPE_OPERATOR_OR,
    COMMAND_TYPE_OPERATOR_PIPE,
    COMMAND_TYPE_OPERATOR_APPEND,
    COMMAND_TYPE_OPERATOR_WRITE,
    COMMAND_TYPE_BACKGROUND,
    COMMAND_TYPE_COMMENT,
    COMMAND_TYPE_UNKNOWN,

    COMMAND_TYPE_MAX
} CommandType;

typedef struct {
    uint32_t type;

    size_t argc;
    size_t argCapacity;
    char *name;
    char **argv;
} Command;

typedef struct {
    Command *tokens;
    size_t size;
    size_t capacity;
} CommandArray;

#ifdef __cplusplus
extern "C" {
#endif

CommandArray parser(char **input);
void parser_free(CommandArray *const ptr);

#ifdef __cplusplus
}
#endif
