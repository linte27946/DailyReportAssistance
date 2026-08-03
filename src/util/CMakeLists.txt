add_library(dailyreport_util INTERFACE)

target_include_directories(dailyreport_util INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_util INTERFACE
    Qt6::Core
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)
