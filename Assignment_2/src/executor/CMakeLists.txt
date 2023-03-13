project(executor)
add_library(executor SHARED
        executor.h
        executor.c)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(CMakePrintHelpers)

set_target_properties(executor PROPERTIES LINKER_LANGUAGE C)
set_target_properties(executor PROPERTIES COMPILER_LANGUAGE C)

target_link_libraries(executor parser)
include_directories(${parser_SOURCE_DIR})
