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

# ═══════════════════════════════════════════════════════
#  Convenience targets
# ═══════════════════════════════════════════════════════

# ---- locate Qt plugin dir (vcpkg) --------------------
set(QT_PLUGIN_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/Qt6/plugins"
    CACHE PATH "Qt6 plugins directory")
set(QT_BIN_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
    CACHE PATH "Qt6 runtime DLLs directory")

# ---- deploy : copy Qt plugins & runtime DLLs ---------
add_custom_target(deploy
    COMMENT "Deploying Qt plugins and runtime DLLs..."
)
if(WIN32)
    # platform plugin (must-have)
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGIN_DIR}/platforms/qwindows$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/platforms/"
    )
    # SQL drivers
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGIN_DIR}/sqldrivers/qsqlite$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/sqldrivers/"
    )
    # styles
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGIN_DIR}/styles/qmodernwindowsstyle$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/styles/"
    )
    # image formats (jpeg, gif, ico)
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGIN_DIR}/imageformats/qjpeg$<$<CONFIG:Debug>:d>.dll"
            "${QT_PLUGIN_DIR}/imageformats/qgif$<$<CONFIG:Debug>:d>.dll"
            "${QT_PLUGIN_DIR}/imageformats/qico$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/imageformats/"
    )
    # TLS backends (needed for QtNetwork / HTTPS)
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/tls"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGIN_DIR}/tls/qschannelbackend$<$<CONFIG:Debug>:d>.dll"
            "${QT_PLUGIN_DIR}/tls/qopensslbackend$<$<CONFIG:Debug>:d>.dll"
            "${QT_PLUGIN_DIR}/tls/qcertonlybackend$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/tls/"
    )
    # network information
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DailyReport>/networkinformation"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGIN_DIR}/networkinformation/qnetworklistmanager$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/networkinformation/"
    )
    # extra runtime DLLs (Qt6Network, OpenSSL, etc.)
    add_custom_command(TARGET deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_BIN_DIR}/Qt6Network$<$<CONFIG:Debug>:d>.dll"
            "${QT_BIN_DIR}/Qt6DBus$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:DailyReport>/"
    )
    message(STATUS "Deploy target configured — run: cmake --build build --target deploy")
endif()

# ---- run : build + deploy + launch -------------------
add_custom_target(run
    COMMENT "Building, deploying, and launching DailyReport..."
)
if(WIN32)
    add_custom_command(TARGET run POST_BUILD
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG>
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG> --target deploy
        COMMAND "$<TARGET_FILE:DailyReport>"
    )
endif()

# ---- check : build + run tests -----------------------
add_custom_target(check
    COMMENT "Building and running tests..."
)
add_custom_command(TARGET check POST_BUILD
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config $<CONFIG>
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR}
        --build-config $<CONFIG> --output-on-failure
)

# ---- clean-all : remove build outputs, keep CMake cache
add_custom_target(clean-all
    COMMENT "Cleaning build outputs (keeping CMake cache)..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${CMAKE_BINARY_DIR}/Release"
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${CMAKE_BINARY_DIR}/Debug"
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${CMAKE_BINARY_DIR}/RelWithDebInfo"
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${CMAKE_BINARY_DIR}/MinSizeRel"
    COMMAND ${CMAKE_COMMAND} -E echo "Build cleaned. CMake cache preserved."
    COMMAND ${CMAKE_COMMAND} -E echo "Re-run: cmake --build build --config Release"
)

# ---- distclean : remove entire build directory --------
add_custom_target(distclean
    COMMENT "Removing entire build directory..."
    COMMAND ${CMAKE_COMMAND} -E echo "Removing ${CMAKE_BINARY_DIR} ..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Done."
    COMMAND ${CMAKE_COMMAND} -E echo "Re-run: cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=..."
)

# ---- format : clang-format all source files ----------
find_program(CLANG_FORMAT NAMES clang-format clang-format-18 clang-format-17)
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        "${CMAKE_SOURCE_DIR}/src/**/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/**/*.h"
        "${CMAKE_SOURCE_DIR}/tests/**/*.cpp"
        "${CMAKE_SOURCE_DIR}/tests/**/*.h"
    )
    add_custom_target(format
        COMMENT "Formatting source files with clang-format..."
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
    )
    message(STATUS "Format target configured — run: cmake --build build --target format")
else()
    message(STATUS "clang-format not found, 'format' target disabled")
endif()

# ---- targets : print available convenience targets ------
add_custom_target(targets
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "  Available convenience targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "    deploy      Copy Qt plugins + DLLs to output directory"
    COMMAND ${CMAKE_COMMAND} -E echo "    run         Build + deploy + launch the app"
    COMMAND ${CMAKE_COMMAND} -E echo "    check       Build + run unit tests"
    COMMAND ${CMAKE_COMMAND} -E echo "    clean-all   Remove build outputs, keep CMake cache"
    COMMAND ${CMAKE_COMMAND} -E echo "    distclean   Remove entire build directory"
    COMMAND ${CMAKE_COMMAND} -E echo "    format      Run clang-format on all source files"
    COMMAND ${CMAKE_COMMAND} -E echo "    targets     Show this message"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)
