add_library(minesleuth_core STATIC
    parser/log_parser.cpp
    parser/stack_trace_parser.cpp
    parser/crash_report_parser.cpp
    parser/input_detector.cpp
    analyzer/exception_extractor.cpp
    analyzer/error_grouper.cpp
    analyzer/log_analyzer.cpp
    analyzer/mod_attribution.cpp
    diagnostics/rule_engine.cpp
    diagnostics/default_rules.cpp
)

target_include_directories(minesleuth_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(minesleuth_core PUBLIC minesleuth_warnings)

add_executable(minesleuth main.cpp)
target_link_libraries(minesleuth PRIVATE minesleuth_core)
