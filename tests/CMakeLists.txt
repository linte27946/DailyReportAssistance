function(add_dailyreport_test name sources)
    add_executable(${name} ${sources})
    target_link_libraries(${name} PRIVATE
        dailyreport_core
        dailyreport_storage
        dailyreport_pipeline
        dailyreport_llm
        dailyreport_report
        dailyreport_util
        Qt6::Core Qt6::Test Qt6::Sql Qt6::EntryPointPrivate
        spdlog::spdlog
    )
    if(WIN32)
        set_target_properties(${name} PROPERTIES
            WIN32_EXECUTABLE FALSE
        )
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_dailyreport_test(test_event_classifier test_event_classifier.cpp)
add_dailyreport_test(test_pipeline test_pipeline.cpp)
add_dailyreport_test(test_database test_database.cpp)
add_dailyreport_test(test_template_engine test_template_engine.cpp)
