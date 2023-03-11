project(parser)
add_library(parser SHARED
        parser.h
        parser.c)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(CMakePrintHelpers)

set_target_properties(parser PROPERTIES LINKER_LANGUAGE C)
set_target_properties(parser PROPERTIES COMPILER_LANGUAGE C)