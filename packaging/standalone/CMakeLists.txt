cmake_minimum_required(VERSION 3.18)
project(lxqt-resource-monitor VERSION 1.0.8 LANGUAGES CXX)

if(NOT LXQT_PANEL_SOURCE_DIR
   OR NOT EXISTS "${LXQT_PANEL_SOURCE_DIR}/panel/ilxqtpanelplugin.h")
    message(FATAL_ERROR "Set LXQT_PANEL_SOURCE_DIR to an LXQt Panel source tree")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets)
find_package(lxqt 2.3 REQUIRED)
find_path(STATGRAB_INCLUDE_DIR statgrab.h REQUIRED)
find_library(STATGRAB_LIBRARY NAMES statgrab REQUIRED)

set(STATGRAB_LIB "${STATGRAB_LIBRARY}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
add_subdirectory(
    "${LXQT_PANEL_SOURCE_DIR}/plugin-resourcemonitor"
    "${CMAKE_BINARY_DIR}/plugin-resourcemonitor"
)
