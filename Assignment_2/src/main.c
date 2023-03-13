#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include "parser/parser.h"
#include "executor/executor.h"

#define INPUT_SIZE 1024

#ifdef DEBUG
#define LOG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

static bool checkClosedParenthesis(char *const input, size_t len) {
    bool opened = false;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '"' && (i == 0 || input[i - 1] != '\\')) {
            opened = !opened;
        }
    }
    return !opened;
}

int main(int argc, char **argv)
{
    while (true) {
        char *input = NULL;
        CommandArray parsed = parser(&input); // -1 required to exclude newline
//        for (size_t i = 0; i < parsed.size; i++) {
//            printf("[%lu] %s\n", i, parsed.tokens[i].name);
//            for (size_t k = 0; k < parsed.tokens[i].argc; k++) {
//                printf("[%lu] %s", k, parsed.tokens[i].argv[k]);
//            }
//        }
//        LOG("Input: %s", input);
//        LOG("Parsed %u comamnds", parsed.tokenCount);

        execute(parsed);

//        free(input);
//        parser_free(&parsed);
    }
}