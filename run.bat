@echo off
REM DailyReport convenience launcher
REM Usage: run.bat [target]
REM   no args  - build, deploy, and run
REM   deploy   - build and copy runtime dependencies
REM   check    - build and run tests
REM   clean    - remove build outputs but keep the cache
REM   distclean - remove the entire build directory
REM   targets  - list available build targets

set TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

REM The linker cannot replace the executable while DailyReport is running.
if not "%1"=="" if /I not "%1"=="deploy" goto :skip_running_check
tasklist /FI "IMAGENAME eq DailyReport.exe" 2>NUL | find /I "DailyReport.exe" >NUL
if not errorlevel 1 (
    echo DailyReport is still running.
    echo Exit it from the system tray, then run this script again.
    exit /b 2
)
:skip_running_check

if "%1"=="deploy" (
    cmake --build build --config Release --target deploy
    if errorlevel 1 exit /b 1
    goto :eof
)
if "%1"=="check" (
    cmake --build build --config Release --target check
    if errorlevel 1 exit /b 1
    goto :eof
)
if "%1"=="clean" (
    cmake --build build --config Release --target clean-all
    if errorlevel 1 exit /b 1
    goto :eof
)
if "%1"=="distclean" (
    cmake --build build --target distclean
    if errorlevel 1 exit /b 1
    goto :eof
)
if "%1"=="targets" (
    cmake --build build --target targets
    if errorlevel 1 exit /b 1
    goto :eof
)
if "%1"=="reconf" (
    cmake -B build -S . %TOOLCHAIN%
    if errorlevel 1 exit /b 1
    goto :eof
)

REM default: build + deploy + run
cmake --build build --config Release --target deploy
if errorlevel 1 exit /b 1
start "" "build\Release\DailyReport.exe"
