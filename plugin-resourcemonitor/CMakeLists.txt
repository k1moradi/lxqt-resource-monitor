set(PLUGIN "resourcemonitor")

set(HEADERS
    lxqtresourcemonitorplugin.h
    lxqtresourcemonitor.h
    lxqtresourcemonitorconfiguration.h
    resourcemonitormath.h
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

BUILD_LXQT_PLUGIN(${PLUGIN})

add_executable(resourcemonitor-netcap resourcemonitor-netcap.cpp)
target_compile_features(resourcemonitor-netcap PRIVATE cxx_std_17)
target_compile_options(resourcemonitor-netcap PRIVATE -Wall -Wextra -Wpedantic)
install(TARGETS resourcemonitor-netcap RUNTIME DESTINATION bin)
