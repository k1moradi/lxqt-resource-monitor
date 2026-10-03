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
