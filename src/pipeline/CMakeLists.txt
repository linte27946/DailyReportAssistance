add_library(dailyreport_pipeline STATIC
    EventPipeline.cpp
    EventCollector.cpp
    EventFilter.cpp
    ActivityClassifier.cpp
    TimelineAssembler.cpp
    PipelineWorker.cpp
)

target_include_directories(dailyreport_pipeline PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_pipeline PUBLIC
    dailyreport_core
    dailyreport_util
    Qt6::Core
)
