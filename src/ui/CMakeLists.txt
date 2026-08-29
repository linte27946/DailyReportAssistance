add_library(dailyreport_ui STATIC
    UiLanguage.cpp
    SystemTray.cpp
    MainWindow.cpp
    SettingsDialog.cpp
    ReportViewer.cpp
    ReportHistoryWidget.cpp
    TimelineWidget.cpp
    SetupWizard.cpp
)

target_include_directories(dailyreport_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_ui PUBLIC
    dailyreport_app
    dailyreport_report
    dailyreport_storage
    dailyreport_util
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)
