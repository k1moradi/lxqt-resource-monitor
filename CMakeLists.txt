cmake_minimum_required(VERSION 3.18)
project(lxqt-resource-monitor VERSION 1.0.8 LANGUAGES CXX)

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Core)
    find_path(STATGRAB_INCLUDE_DIR statgrab.h REQUIRED)
    find_library(STATGRAB_LIBRARY NAMES statgrab REQUIRED)

    add_library(resourcemonitormath STATIC
        plugin-resourcemonitor/resourcemonitormath.cpp
    )
    target_compile_features(resourcemonitormath PUBLIC cxx_std_17)
    target_include_directories(resourcemonitormath
        PUBLIC plugin-resourcemonitor ${STATGRAB_INCLUDE_DIR}
    )
    target_link_libraries(resourcemonitormath PUBLIC Qt6::Core ${STATGRAB_LIBRARY})

    add_executable(resourcemonitor-math-test
        tests/resource_monitor_math_test.cpp
    )
    target_link_libraries(resourcemonitor-math-test PRIVATE resourcemonitormath)

    add_test(NAME resourcemonitor-math COMMAND resourcemonitor-math-test)

    add_executable(resourcemonitor-history-test
        tests/resource_monitor_history_test.cpp
    )
    target_include_directories(resourcemonitor-history-test
        PRIVATE plugin-resourcemonitor
    )
    target_compile_features(resourcemonitor-history-test PRIVATE cxx_std_17)
    add_test(NAME resourcemonitor-history COMMAND resourcemonitor-history-test)

    add_executable(resourcemonitor-packet-test
        tests/resource_monitor_packet_test.cpp
    )
    target_include_directories(resourcemonitor-packet-test
        PRIVATE plugin-resourcemonitor
    )
    target_compile_features(resourcemonitor-packet-test PRIVATE cxx_std_17)
    add_test(NAME resourcemonitor-packet COMMAND resourcemonitor-packet-test)

    add_executable(resourcemonitor-cpu-sampling-test
        tests/resource_monitor_cpu_sampling_test.cpp
    )
    target_link_libraries(resourcemonitor-cpu-sampling-test
        PRIVATE resourcemonitormath
    )
    add_test(NAME resourcemonitor-cpu-sampling COMMAND resourcemonitor-cpu-sampling-test)
    set_tests_properties(resourcemonitor-cpu-sampling PROPERTIES TIMEOUT 10)
endif()
