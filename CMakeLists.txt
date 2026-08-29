cmake_minimum_required(VERSION 3.21)
project(DailyReport VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Default build type ──────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Build type: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
    message(STATUS "Build type not specified, defaulting to Release")
endif()

# ── Platform-specific flags ─────────────────────────
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /MP")
    add_definitions(-DNOMINMAX -DUNICODE -D_UNICODE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
elseif(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -mwindows")
    add_definitions(-DNOMINMAX -DUNICODE -D_UNICODE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
else()
    # Native Unix/Linux build
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# ── Dependencies ────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql Concurrent)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src)

# ── Subdirectories ──────────────────────────────────
add_subdirectory(src/util)
add_subdirectory(src/core)
add_subdirectory(src/storage)
add_subdirectory(src/monitor)
add_subdirectory(src/pipeline)
add_subdirectory(src/llm)
add_subdirectory(src/report)
add_subdirectory(src/app)
add_subdirectory(src/ui)

# ── Resources ───────────────────────────────────────
qt_add_resources(RESOURCES_QRC src/ui/resources/resources.qrc)

# ── Main executable ─────────────────────────────────
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
    Qt6::Core Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Concurrent
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Qt6::EntryPointPrivate only exists on Windows (MSVC/MINGW)
if(WIN32)
    target_link_libraries(DailyReport PRIVATE Qt6::EntryPointPrivate)
endif()

# ── Windows system libraries ────────────────────────
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

# ── Install ─────────────────────────────────────────
install(TARGETS DailyReport RUNTIME DESTINATION bin)
install(DIRECTORY src/storage/migrations/
        DESTINATION share/dailyreport/migrations
        FILES_MATCHING PATTERN "*.sql")
install(FILES src/ui/resources/icons/dailyreport.svg
        DESTINATION share/icons/hicolor/scalable/apps)
install(FILES packaging/dailyreport.desktop
        DESTINATION share/applications)

# ── Tests ───────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# ═══════════════════════════════════════════════════════
#  Convenience targets
# ═══════════════════════════════════════════════════════

# ---- deploy : copy Qt plugins & runtime DLLs -------
if(WIN32)
    if(MINGW)
        set(VCPKG_TRIPLET "x64-mingw-static")
    else()
        set(VCPKG_TRIPLET "x64-windows")
    endif()
endif()

if(WIN32)
    set(QT_PLUGIN_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TRIPLET}/Qt6/plugins"
        CACHE PATH "Qt6 plugins directory")
    set(QT_DEBUG_PLUGIN_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TRIPLET}/debug/Qt6/plugins"
        CACHE PATH "Qt6 debug plugins directory")
    set(QT_BIN_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TRIPLET}/bin"
        CACHE PATH "Qt6 runtime DLLs directory")
    set(QT_DEBUG_BIN_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TRIPLET}/debug/bin"
        CACHE PATH "Qt6 debug runtime DLLs directory")
    set(QT_ACTIVE_PLUGIN_DIR
        "$<IF:$<CONFIG:Debug>,${QT_DEBUG_PLUGIN_DIR},${QT_PLUGIN_DIR}>")
    set(QT_ACTIVE_BIN_DIR
        "$<IF:$<CONFIG:Debug>,${QT_DEBUG_BIN_DIR},${QT_BIN_DIR}>")

    add_custom_target(deploy
        COMMENT "Deploying Qt plugins and runtime DLLs..."
    )
    add_dependencies(deploy DailyReport)

    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ACTIVE_PLUGIN_DIR}/platforms/qwindows$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/platforms/"
    )
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ACTIVE_PLUGIN_DIR}/sqldrivers/qsqlite$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/sqldrivers/"
    )
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ACTIVE_PLUGIN_DIR}/styles/qmodernwindowsstyle$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/styles/"
    )
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ACTIVE_PLUGIN_DIR}/imageformats/qjpeg$<$<CONFIG:Debug>:d>.dll"
            "${QT_ACTIVE_PLUGIN_DIR}/imageformats/qgif$<$<CONFIG:Debug>:d>.dll"
            "${QT_ACTIVE_PLUGIN_DIR}/imageformats/qico$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/imageformats/"
    )
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/tls"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ACTIVE_PLUGIN_DIR}/tls/qschannelbackend$<$<CONFIG:Debug>:d>.dll"
            "${QT_ACTIVE_PLUGIN_DIR}/tls/qopensslbackend$<$<CONFIG:Debug>:d>.dll"
            "${QT_ACTIVE_PLUGIN_DIR}/tls/qcertonlybackend$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/tls/"
    )
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/networkinformation"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ACTIVE_PLUGIN_DIR}/networkinformation/qnetworklistmanager$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/networkinformation/"
    )
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_ACTIVE_BIN_DIR}/Qt6Network$<$<CONFIG:Debug>:d>.dll"
            "${QT_ACTIVE_BIN_DIR}/Qt6DBus$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/"
    )
    message(STATUS "Deploy target configured — run: cmake --build build --target deploy")
endif()

# ---- clean-all : remove build outputs, keep cache---
add_custom_target(clean-all
    COMMENT "Cleaning build outputs (keeping CMake cache)..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/Release"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/Debug"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/RelWithDebInfo"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/MinSizeRel"
    COMMAND ${CMAKE_COMMAND} -E echo "Build cleaned. CMake cache preserved."
)

# ---- distclean --------------------------------------
add_custom_target(distclean
    COMMENT "Removing entire build directory..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Done."
)

# ---- format -----------------------------------------
find_program(CLANG_FORMAT NAMES clang-format clang-format-18 clang-format-17)
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/src/**/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/**/*.h"
        "${CMAKE_SOURCE_DIR}/tests/**/*.cpp"
        "${CMAKE_SOURCE_DIR}/tests/**/*.h"
    )
    add_custom_target(format
        COMMENT "Formatting source files..."
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
    )
    message(STATUS "format target configured")
endif()

# ---- targets ----------------------------------------
add_custom_target(targets
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "  Available targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "    deploy      Copy Qt plugins + DLLs to output"
    COMMAND ${CMAKE_COMMAND} -E echo "    clean-all   Remove build outputs, keep CMake cache"
    COMMAND ${CMAKE_COMMAND} -E echo "    distclean   Remove entire build directory"
    COMMAND ${CMAKE_COMMAND} -E echo "    format      Run clang-format on all sources"
    COMMAND ${CMAKE_COMMAND} -E echo "    targets     Show this message"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)
