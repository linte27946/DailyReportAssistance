add_library(dailyreport_storage INTERFACE)

target_include_directories(dailyreport_storage INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_storage INTERFACE
    dailyreport_core
    Qt6::Core
    Qt6::Sql
)
