include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(minesleuth_tests
    stack_trace_parser_test.cpp
    log_parser_test.cpp
    crash_report_parser_test.cpp
    exception_extractor_test.cpp
    error_grouper_test.cpp
    rules_test.cpp
    mod_attribution_test.cpp
)

target_link_libraries(minesleuth_tests PRIVATE minesleuth_core GTest::gtest_main)
target_include_directories(minesleuth_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(minesleuth_tests PRIVATE
    MINESLEUTH_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

include(GoogleTest)
gtest_discover_tests(minesleuth_tests)
