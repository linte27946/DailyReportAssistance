add_library(dailyreport_app STATIC
    DataRetentionService.cpp
)

target_include_directories(dailyreport_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_app PUBLIC
    dailyreport_storage
    Qt6::Core
    Qt6::Widgets
)
