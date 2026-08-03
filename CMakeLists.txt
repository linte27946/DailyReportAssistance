cmake_minimum_required(VERSION 3.21)
project(DailyReport VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows-specific flags
if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /MP")
    add_definitions(-DNOMINMAX -DUNICODE -D_UNICODE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

# Dependencies via find_package
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

# Include paths
include_directories(${CMAKE_SOURCE_DIR}/src)

# Subdirectories in dependency order
add_subdirectory(src/util)
add_subdirectory(src/core)
add_subdirectory(src/storage)
add_subdirectory(src/monitor)
add_subdirectory(src/pipeline)
add_subdirectory(src/llm)
add_subdirectory(src/report)
add_subdirectory(src/app)
add_subdirectory(src/ui)

# Resource file
qt_add_resources(RESOURCES_QRC src/ui/resources/resources.qrc)

# Main executable
add_executable(DailyReport
    src/main.cpp
    src/app/Application.h
    ${RESOURCES_QRC}
)

target_link_libraries(DailyReport PRIVATE
    dailyreport_app
    dailyreport_ui
    dailyreport_report
    dailyreport_llm
    dailyreport_pipeline
    dailyreport_monitor
    dailyreport_storage
    dailyreport_core
    dailyreport_util
    Qt6::Core Qt6::Widgets Qt6::Network Qt6::Sql Qt6::EntryPointPrivate
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Windows-specific system libraries
if(WIN32)
    target_link_libraries(DailyReport PRIVATE
        advapi32      # DPAPI (CryptProtectData)
        oleacc        # UI Automation
        ole32 oleaut32 # COM
        user32        # SetWinEventHook, GetLastInputInfo
        wevtapi       # ETW
        wbemuuid      # WMI
    )
endif()

# Install rules
install(TARGETS DailyReport RUNTIME DESTINATION bin)

# Tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
