cmake_minimum_required(VERSION 3.15)
project(MineSleuth VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(MINESLEUTH_BUILD_TESTS "Build the unit tests" ON)

add_library(minesleuth_warnings INTERFACE)
if(MSVC)
    target_compile_options(minesleuth_warnings INTERFACE /W4)
else()
    target_compile_options(minesleuth_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src)

if(MINESLEUTH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
