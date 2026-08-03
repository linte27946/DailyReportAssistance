add_library(dailyreport_core INTERFACE)

target_include_directories(dailyreport_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_core INTERFACE
    dailyreport_util
    Qt6::Core
)
