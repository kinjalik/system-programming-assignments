# Add Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/50e07d1c92875e66138d5d5ee3bb46ef237115bb.zip
)
FetchContent_MakeAvailable(googletest)

# Define testing project
enable_testing()
add_executable(tests
        components/parser_сt.cpp
        units/parser_ut.cpp)
target_link_libraries(tests GTest::gtest_main)

set(CMAKE_CXX_STANDARD 14)

include(GoogleTest)
gtest_discover_tests(tests)

# Link the parser
target_link_libraries(tests parser)
include_directories(${parser_SOURCE_DIR})

include(CMakePrintHelpers)
cmake_print_variables(CMAKE_BUILD_TYPE)
