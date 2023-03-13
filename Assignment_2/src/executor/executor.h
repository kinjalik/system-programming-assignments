#pragma once
#include <stdint.h>
#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RESOLVED_NAME_MAX_LEN 128

size_t execute(CommandArray array);

#ifdef __cplusplus
}
#endif
