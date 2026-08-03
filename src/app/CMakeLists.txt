add_library(dailyreport_app INTERFACE)

target_include_directories(dailyreport_app INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_app INTERFACE
    dailyreport_storage
    Qt6::Core
    Qt6::Widgets
)
