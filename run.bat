@echo off
REM DailyReport convenience launcher
REM Usage: run.bat [target]
REM   no args  - build, deploy, and run
REM   deploy   - build and copy runtime dependencies
REM   package  - create a Windows Setup EXE in dist
REM   portable - create a redistributable Windows ZIP in dist
REM   check    - build and run tests
REM   clean    - remove build outputs but keep the cache
REM   distclean - remove the entire build directory
REM   targets  - list available build targets

set TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

REM The linker cannot replace the executable while DailyReport is running.
if "%1"=="" goto :check_running
if /I "%1"=="deploy" goto :check_running
if /I "%1"=="package" goto :check_running
if /I "%1"=="portable" goto :check_running
goto :skip_running_check
:check_running
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
if "%1"=="package" (
    cmake --build build --config Release --target package-installer
    if errorlevel 1 exit /b 1
    echo.
    echo Installer created in dist\DailyReport-Setup-1.0.0.exe
    goto :eof
)
if "%1"=="portable" (
    cmake --build build --config Release --target package-portable
    if errorlevel 1 exit /b 1
    echo.
    echo Portable package created in dist\DailyReport-1.0.0-windows-x64.zip
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
