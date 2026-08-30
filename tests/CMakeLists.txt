function(add_dailyreport_test name sources)
    add_executable(${name} ${sources})
    target_link_libraries(${name} PRIVATE
        dailyreport_core
        dailyreport_storage
        dailyreport_pipeline
        dailyreport_monitor
        dailyreport_llm
        dailyreport_report
        dailyreport_util
        Qt6::Core Qt6::Test Qt6::Sql
        spdlog::spdlog
    )
    if(WIN32)
        target_link_libraries(${name} PRIVATE Qt6::EntryPointPrivate)
        set_target_properties(${name} PROPERTIES
            WIN32_EXECUTABLE FALSE
        )
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_dailyreport_test(test_event_classifier test_event_classifier.cpp)
add_dailyreport_test(test_pipeline test_pipeline.cpp)
add_dailyreport_test(test_database test_database.cpp)
target_link_libraries(test_database PRIVATE dailyreport_app)
if(WIN32 AND TARGET Qt6::QWindowsIntegrationPlugin)
    add_custom_command(TARGET test_database POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:test_database>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:test_database>/platforms/"
    )
endif()
add_dailyreport_test(test_template_engine test_template_engine.cpp)
add_dailyreport_test(test_file_system_monitor test_file_system_monitor.cpp)
add_dailyreport_test(test_work_context test_work_context.cpp)
add_dailyreport_test(test_wecom_meeting_monitor test_wecom_meeting_monitor.cpp)

add_executable(test_ui_language test_ui_language.cpp)
target_link_libraries(test_ui_language PRIVATE
    dailyreport_ui
    Qt6::Core Qt6::Widgets Qt6::Test
)
if(WIN32)
    target_link_libraries(test_ui_language PRIVATE Qt6::EntryPointPrivate)
    set_target_properties(test_ui_language PROPERTIES WIN32_EXECUTABLE FALSE)
endif()
add_test(NAME test_ui_language COMMAND test_ui_language)
if(WIN32 AND TARGET Qt6::QWindowsIntegrationPlugin)
    add_custom_command(TARGET test_ui_language POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:test_ui_language>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:test_ui_language>/platforms/"
    )
elseif(TARGET Qt6::QOffscreenIntegrationPlugin)
    add_custom_command(TARGET test_ui_language POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:test_ui_language>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
            "$<TARGET_FILE_DIR:test_ui_language>/platforms/"
    )
    set_tests_properties(test_ui_language PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

if(TARGET Qt6::QSQLiteDriverPlugin)
    add_custom_command(TARGET test_database POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:test_database>/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QSQLiteDriverPlugin>"
            "$<TARGET_FILE_DIR:test_database>/sqldrivers/"
    )
endif()
