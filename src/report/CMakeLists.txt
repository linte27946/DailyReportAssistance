add_library(dailyreport_report STATIC
    ReportGenerator.cpp
    TemplateEngine.cpp
    MarkdownRenderer.cpp
    ReportScheduler.cpp
)

target_include_directories(dailyreport_report PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_report PUBLIC
    dailyreport_core
    dailyreport_storage
    dailyreport_llm
    dailyreport_util
    Qt6::Core
    Qt6::Concurrent
)
