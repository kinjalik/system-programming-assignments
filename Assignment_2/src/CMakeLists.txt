add_executable(output main.c)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set_target_properties(output PROPERTIES LINKER_LANGUAGE C)
set_target_properties(output PROPERTIES COMPILER_LANGUAGE C)

add_subdirectory(parser)
add_subdirectory(executor)
target_link_libraries(output parser executor)
include_directories(${parser_SOURCE_DIR})
include_directories(${executor_SOURCE_DIR})
