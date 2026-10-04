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

# Keep x86-64 builds usable on older processors: permit SSSE3, but do not let
# a build host with a newer CPU introduce SSE4.1, AVX, or later instructions.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"
   AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(RESOURCEMONITOR_X86_FLAGS
        -march=x86-64
        -mtune=generic
        -mssse3
        -mno-sse4.1
        -mno-sse4.2
        -mno-avx
    )
    target_compile_options(resourcemonitor PRIVATE ${RESOURCEMONITOR_X86_FLAGS})
    target_compile_options(resourcemonitor-netcap PRIVATE ${RESOURCEMONITOR_X86_FLAGS})
endif()

install(TARGETS resourcemonitor-netcap RUNTIME DESTINATION bin)
