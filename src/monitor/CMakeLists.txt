add_library(dailyreport_monitor STATIC
    MonitorEngine.cpp
    IMonitor.h
    FileSystemMonitor.cpp
    ProcessMonitor.cpp
    WindowFocusMonitor.cpp
    InputActivityMonitor.cpp
    BrowserUrlMonitor.cpp
    GitMonitor.cpp
    BuildMonitor.cpp
)

target_include_directories(dailyreport_monitor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dailyreport_monitor PUBLIC
    dailyreport_core
    Qt6::Core
)

if(WIN32)
    target_link_libraries(dailyreport_monitor PUBLIC
        advapi32
        oleacc
        ole32 oleaut32
        user32
    )
endif()
