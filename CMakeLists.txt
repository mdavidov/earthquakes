cmake_minimum_required(VERSION 3.20)
project(EarthquakeAlertSystem)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH)
    message(WARNING "\nWARNING: CMAKE_PREFIX_PATH must be defined "
            "as path to your Qt installation, "
            "e.g. 'cmake .. -DCMAKE_PREFIX_PATH=C:/Qt/6.9.1/msvc2022_64 ...' "
            "(or /opt/Qt/6.9.1/macos). Using DEFAULT for Qt 6.9.1.")
    if(APPLE)
        set(CMAKE_PREFIX_PATH "/opt/Qt/6.9.1/macos")
        set(CMAKE_MAKE_PROGRAM /opt/homebrew/bin/ninja)  # this is for Qt Creator
    elseif(UNIX)
        set(CMAKE_MAKE_PROGRAM "/opt/Qt/6.8.1/gcc_arm64")
    elseif(WIN32)
        set(CMAKE_PREFIX_PATH "C:/Qt/6.8.1/msvc2022_64")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Sql
    # Location
    Positioning
)

add_executable(EarthquakeAlertSystem
    src/main.cpp
    src/earthquake_application.cpp
    src/earthquake_data.cpp
    src/earthquake_database.cpp
    src/earthquake_api_client.cpp
    src/earthquake_map_widget.cpp
    src/earthquake_main_window.cpp
    src/geojson_parser.cpp
    src/notification_manager.cpp
    src/spatial_utils.cpp
)

target_link_libraries(EarthquakeAlertSystem
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    # Qt6::Location
    Qt6::Positioning
)
