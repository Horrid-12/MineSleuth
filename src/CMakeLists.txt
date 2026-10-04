add_executable(minesleuth
    main.cpp
    parser/log_parser.cpp
)

target_include_directories(minesleuth PRIVATE .)
