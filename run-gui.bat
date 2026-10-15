@echo off
setlocal
set "APP_DIR=%~dp0build\src\gui"
set "APP_EXE=%APP_DIR%\minesleuth_gui.exe"

if not exist "%APP_EXE%" (
    echo [ERROR] MineSleuth GUI executable not found at "%APP_EXE%".
    echo Please build the project with CMake and Qt 6 first.
    pause
    exit /b 1
)

if "%~1"=="" (
    start "" /D "%APP_DIR%" "%APP_EXE%"
) else (
    start "" /D "%APP_DIR%" "%APP_EXE%" "%~f1"
)
exit /b 0
