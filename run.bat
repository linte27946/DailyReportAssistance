@echo off
REM DailyReport — convenience launcher
REM Usage: run.bat [target]
REM   (no args)  → cmake --build build --config Release --target deploy && run
REM   deploy     → cmake --build build --config Release --target deploy
REM   check      → cmake --build build --config Release --target check
REM   clean      → cmake --build build --config Release --target clean-all
REM   distclean  → cmake --build build --target distclean
REM   targets    → cmake --build build --target targets

set TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

if "%1"=="deploy" (
    cmake --build build --config Release --target deploy
    goto :eof
)
if "%1"=="check" (
    cmake --build build --config Release --target check
    goto :eof
)
if "%1"=="clean" (
    cmake --build build --config Release --target clean-all
    goto :eof
)
if "%1"=="distclean" (
    cmake --build build --target distclean
    goto :eof
)
if "%1"=="targets" (
    cmake --build build --target targets
    goto :eof
)
if "%1"=="reconf" (
    cmake -B build -S . %TOOLCHAIN%
    goto :eof
)

REM default: build + deploy + run
cmake --build build --config Release --target deploy
start "" "build\Release\DailyReport.exe"
