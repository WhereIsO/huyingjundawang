@echo off
setlocal enabledelayedexpansion

rem MSVC + Qt 5.15.2 build helper.
rem The course directory contains Chinese characters. Qt AUTOMOC may generate
rem broken include paths there, so this script mirrors the project to a short
rem ASCII path and builds from that mirror.

set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set "SRC=%~dp0"
if "%SRC:~-1%"=="\" set "SRC=%SRC:~0,-1%"
set "ASCII_SRC=G:\_src_backup_tool_gui"
set "BUILD=G:\_build_backup_tool_gui_ascii"
set "QT_PREFIX=G:\Anaconda3\Library"

if not exist "%VCVARS%" (
    echo [ERROR] Cannot find vcvars64.bat: %VCVARS%
    exit /b 1
)

echo [INFO] Mirroring source to %ASCII_SRC%
if exist "%ASCII_SRC%" rmdir /s /q "%ASCII_SRC%"
robocopy "%SRC%" "%ASCII_SRC%" /MIR /XD build .git >nul
if errorlevel 8 (
    echo [ERROR] robocopy failed.
    exit /b 1
)

call "%VCVARS%" || exit /b 1

echo [INFO] Configuring CMake
cmake -S "%ASCII_SRC%" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DCMAKE_CXX_COMPILER=cl || exit /b 1

echo [INFO] Building
cmake --build "%BUILD%" || exit /b 1

echo [INFO] Running tests
ctest --test-dir "%BUILD%" --output-on-failure || exit /b 1

echo [OK] Build and tests completed.
echo [OK] Executable: %BUILD%\BackupTool.exe
endlocal
