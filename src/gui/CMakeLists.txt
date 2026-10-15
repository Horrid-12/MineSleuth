cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

add_executable(minesleuth_gui
    main.cpp
    main_window.cpp
    main_window.h
    diagnostic_hero.cpp
    diagnostic_hero.h
    action_sidebar.cpp
    action_sidebar.h
    log_model.cpp
    log_model.h
    resources/resources.qrc
)

target_include_directories(minesleuth_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(minesleuth_gui PRIVATE
    minesleuth_core
    Qt6::Widgets
)

if(MINGW)
    target_link_options(minesleuth_gui PRIVATE -Wl,--allow-multiple-definition)
endif()

if(WIN32)
    # Hide the background console window on release builds
    # set_target_properties(minesleuth_gui PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
