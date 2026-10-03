cmake_minimum_required(VERSION 3.18)
project(lxqt-resource-monitor VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Core)

    add_executable(resourcemonitor-math-test
        tests/resource_monitor_math_test.cpp
        plugin-resourcemonitor/resourcemonitormath.cpp
    )
    target_compile_features(resourcemonitor-math-test PRIVATE cxx_std_17)
    target_include_directories(resourcemonitor-math-test PRIVATE plugin-resourcemonitor)
    target_link_libraries(resourcemonitor-math-test PRIVATE Qt6::Core)

    add_test(NAME resourcemonitor-math COMMAND resourcemonitor-math-test)
endif()
