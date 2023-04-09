//
// Created by kinjalik on 3/10/23.
//
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include "executor.h"

#define PATH "PATH"

static bool resolvePath(const char *name, char resolvedName[RESOLVED_NAME_MAX_LEN]);
static void executeProgram(int p[2], char *resolvedName, Command *token);

#define INITIAL_HANDLER_ARRAY_LEN 8
#define HANDLER_ARRAY_EXTENSION 2
typedef struct {
    int pid;
    int inputFd;
    int outputPipe[2];
} Handler;

typedef struct {
    Handler *arr;
    size_t capacity;
    size_t size;
} HandlerArray;


static HandlerArray arrayInit() {
    Handler *arr = (Handler *) calloc(INITIAL_HANDLER_ARRAY_LEN, sizeof(Handler));
    if (arr == NULL) {
        printf("Failed to allocate array of process handlers");
    }

    HandlerArray ret = {
            .arr = arr,
            .capacity = arr != NULL ? INITIAL_HANDLER_ARRAY_LEN : 0,
            .size = 0
    };
    return ret;
}

static void extendArray(HandlerArray *hndlr) {
    size_t newCap = hndlr->capacity + HANDLER_ARRAY_EXTENSION;
    Handler *newArr = (Handler *) calloc(newCap, sizeof(Handler));
    if (newArr == NULL) {
        printf("Failed to extend array");
        return;
    }
    memcpy(newArr, hndlr->arr, sizeof(Handler) * hndlr->size);
    free(hndlr->arr);
    hndlr->arr = newArr;
    hndlr->capacity = newCap;
}

static void arrayFree(HandlerArray *h) {
    free(h->arr);
    h->size = 0;
    h->capacity = 0;
}

static int executeRegular(Command *token, HandlerArray *hndlrs) {
    char resolvedName[RESOLVED_NAME_MAX_LEN] = {0};
    bool resolved = resolvePath(token->name, resolvedName);

    if (hndlrs->size == hndlrs->capacity) {
        extendArray(hndlrs);
    }
    Handler *h = &hndlrs->arr[hndlrs->size];

    h->inputFd = hndlrs->size > 0 ? hndlrs->arr[hndlrs->size - 1].outputPipe[0] : STDIN_FILENO;
    hndlrs->size++;
    pipe(h->outputPipe);

    int pid = fork();
    if (pid == 0) {
        if (h->inputFd != STDIN_FILENO) {
            printf("pid %u, change STDIN to %d", pid, h->inputFd);
            dup2(h->inputFd, 0);
        }

        close(h->outputPipe[0]);
        static bool flag = false;
        dup2(h->outputPipe[1], 1);
        dup2(h->outputPipe[1], 2);
        close(h->outputPipe[1]);
        if (resolved) {
            execv(resolvedName, token->argv);
        } else {
            printf("Command not found");
        }
        exit(0);
    } else {
        close(h->outputPipe[1]);
        h->pid = pid;
    }
    return EXIT_SUCCESS;
}

static int syncPointProc(HandlerArray *hndlrs, FILE *outFile) {
    if (hndlrs->size == 0)
        return 0;

    Handler *hLast = &hndlrs->arr[--hndlrs->size];
    int status;
    waitpid(hLast->pid, &status, 0);
    int ret = WEXITSTATUS(status);

    for (size_t i = hndlrs->size - 1; i != -1; i--) {
        kill(hndlrs->arr[i].pid, SIGQUIT);
        close(hndlrs->arr[i].outputPipe[0]);
        if (hndlrs->arr[i].inputFd != STDIN_FILENO)
            close(hndlrs->arr[i].inputFd);
        hndlrs->arr[i].pid = 0;
    }

    char output[512] = {0};
    ssize_t readRes;
    int fd = hLast->outputPipe[0];
    do {
        fprintf(outFile, "%s", output);
        readRes = read(fd, output, sizeof(output)-1);
        output[readRes] = 0;
    } while (readRes > 0);

    hndlrs->size = 0;
    hLast->pid = 0;

    return ret;
}

static int syncPointProcStdout(HandlerArray *hndlrs) {
    return syncPointProc(hndlrs, stdout);
}

static int syncPointProcFile(HandlerArray *hndlrs, const char *fileName, bool isAppendMode) {
    FILE *fd = fopen(fileName, isAppendMode ? "a" : "w");
    int ret = syncPointProc(hndlrs, fd);
    fflush(fd);
    fclose(fd);
    return ret;
}

#define BUILTIN_CD "cd"
static uint32_t executeBuiltin(Command *token) {
    if (strcmp(token->name, BUILTIN_CD) == 0) {
        int ret = 0;
        if (token->argc < 1) {
            ret = chdir("/");
        } else {
            ret = chdir(token->argv[1]);
        }
//        if (ret != 0) {
//            printf("ERROR: %s\n", strerror(-ret));
//        }
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

size_t execute(CommandArray array) {
    HandlerArray hndlrs = arrayInit();
    size_t regularsExecuted = 0;

    for (size_t i = 0; i < array.size; i++) {
        Command *token = &array.tokens[i];
        if (token->type == COMMAND_TYPE_COMMENT) {
            break;
        }
        if (token->type == COMMAND_TYPE_REGULAR) {
            uint32_t builtInError = executeBuiltin(token);
            if (builtInError != EXIT_FAILURE) {
                continue;
            }
            uint32_t ret = executeRegular(token, &hndlrs);
            if (ret != EXIT_SUCCESS) {
                printf("failed to execute\n");
                break;
            }
            regularsExecuted++;
        } else if (token->type == COMMAND_TYPE_OPERATOR_PIPE) {
            continue;
        } else if (token->type == COMMAND_TYPE_OPERATOR_AND) {
            int ret = syncPointProcStdout(&hndlrs);
            if (ret != 0)
                break;
        } else if (token->type == COMMAND_TYPE_OPERATOR_OR) {
            int ret = syncPointProcStdout(&hndlrs);
            if (ret == 0)
                break;
        } else if (token->type == COMMAND_TYPE_OPERATOR_APPEND || token->type == COMMAND_TYPE_OPERATOR_WRITE) {
            if (i + 1 >= array.size) {
                printf("No target for write provided");
                continue;
            }
            Command *newToken = &array.tokens[++i];
            syncPointProcFile(&hndlrs, newToken->name, token->type == COMMAND_TYPE_OPERATOR_APPEND);
        }
    }

    syncPointProcStdout(&hndlrs);

    arrayFree(&hndlrs);
    return regularsExecuted;
}

static bool resolvePath(const char *const name, char resolvedName[RESOLVED_NAME_MAX_LEN + 1]) {
    char *path = getenv(PATH);
    size_t pathLen = strlen(path);

    size_t startPos = 0;
    while (startPos < pathLen){
        size_t endPos = startPos;
        while (endPos < pathLen && path[endPos] != ':') endPos++;

        char oldCh = path[endPos];
        path[endPos] = 0;

        char concatedName[RESOLVED_NAME_MAX_LEN] = {0};
        sprintf(concatedName, "%s/%s", &path[startPos], name);
        path[endPos] = oldCh;

        if (access(concatedName, X_OK) == EXIT_SUCCESS) {
//            printf("Resolved using %s\n", concatedName);
            strcpy(resolvedName, concatedName);
            return true;
        }

        if (path[endPos] == 0) {
            break;
        }
        startPos = endPos + 1;
    }
    return false;
}

#ifdef DEBUG
#endif
