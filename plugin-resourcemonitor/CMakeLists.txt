set(PLUGIN "resourcemonitor")

set(HEADERS
    lxqtresourcemonitorplugin.h
    lxqtresourcemonitor.h
    lxqtresourcemonitorconfiguration.h
    resourcemonitormath.h
    resourcemonitorhistory.h
    resourcemonitorpacket.h
)

set(SOURCES
    lxqtresourcemonitorplugin.cpp
    lxqtresourcemonitor.cpp
    lxqtresourcemonitorconfiguration.cpp
    resourcemonitormath.cpp
)

set(UIS
    lxqtresourcemonitorconfiguration.ui
)

set(LIBRARIES ${STATGRAB_LIB})

if(COMMAND BUILD_LXQT_PLUGIN)
    # Full LXQt Panel source-tree build.
    BUILD_LXQT_PLUGIN(${PLUGIN})
else()
    # Lightweight package build: compile only this module against the panel's
    # private headers without configuring the panel or its unrelated plugins.
    if(NOT LXQT_PANEL_SOURCE_DIR
       OR NOT EXISTS "${LXQT_PANEL_SOURCE_DIR}/panel/ilxqtpanelplugin.h")
        message(FATAL_ERROR "Set LXQT_PANEL_SOURCE_DIR to an LXQt Panel source tree")
    endif()

    add_library(${PLUGIN} MODULE ${HEADERS} ${SOURCES} ${UIS})
    target_include_directories(${PLUGIN} PRIVATE
        "${LXQT_PANEL_SOURCE_DIR}/panel"
        ${STATGRAB_INCLUDE_DIR}
    )
    target_compile_features(${PLUGIN} PRIVATE cxx_std_17)
    target_compile_options(${PLUGIN} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(${PLUGIN} PRIVATE Qt6::Widgets lxqt ${LIBRARIES})

    configure_file(resources/resourcemonitor.desktop
        "${CMAKE_CURRENT_BINARY_DIR}/resourcemonitor.desktop" COPYONLY)
endif()

add_executable(resourcemonitor-netcap resourcemonitor-netcap.cpp)
target_compile_features(resourcemonitor-netcap PRIVATE cxx_std_17)
target_compile_options(resourcemonitor-netcap PRIVATE -Wall -Wextra -Wpedantic)

option(RESOURCEMONITOR_NATIVE_OPTIMIZATIONS
    "Optimize for the CPU that is building the resource monitor"
    OFF
)

# Leave the default instruction set to the compiler and toolchain. For builds
# intended only for this machine, -march=native lets the compiler use the
# instruction sets reported by the build CPU without requesting unsupported
# ones.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
   AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$"
   AND RESOURCEMONITOR_NATIVE_OPTIMIZATIONS)
    target_compile_options(resourcemonitor PRIVATE -march=native -mtune=native)
    target_compile_options(resourcemonitor-netcap PRIVATE -march=native -mtune=native)
endif()

install(TARGETS resourcemonitor-netcap RUNTIME DESTINATION bin)
