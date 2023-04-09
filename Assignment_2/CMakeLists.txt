cmake_minimum_required(VERSION 3.22)
project(Assignment_1 C CXX)

set(CMAKE_C_STANDARD 11)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Build for debug!")
    add_definitions(-DDEBUG)
endif()

add_subdirectory(src)
