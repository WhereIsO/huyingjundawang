@echo off
setlocal enabledelayedexpansion

rem Portable MSVC + Qt build helper.
rem Optional environment variables:
rem   VCVARS       full path to vcvars64.bat
rem   QT_PREFIX    Qt/CMake prefix, for example C:\Qt\5.15.2\msvc2019_64
rem   ASCII_SRC    short ASCII mirror directory, defaults to %TEMP%\BackupTool_src
rem   BUILD        build directory, defaults to %TEMP%\BackupTool_build
rem   GENERATOR    CMake generator, defaults to Ninja
rem   RUN_TESTS    1/0, defaults to 1
rem   BACKUP_ALLOW_GTEST_DOWNLOAD  ON/OFF, defaults to OFF

set "SRC=%~dp0"
if "%SRC:~-1%"=="\" set "SRC=%SRC:~0,-1%"

if not defined ASCII_SRC set "ASCII_SRC=%TEMP%\BackupTool_src"
if not defined BUILD set "BUILD=%TEMP%\BackupTool_build"
if not defined GENERATOR set "GENERATOR=Ninja"
if not defined RUN_TESTS set "RUN_TESTS=1"
if not defined BACKUP_ALLOW_GTEST_DOWNLOAD set "BACKUP_ALLOW_GTEST_DOWNLOAD=OFF"

if not defined VCVARS (
    for %%P in (
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    ) do (
        if exist "%%~P" set "VCVARS=%%~P"
    )
)

if not exist "%VCVARS%" (
    echo [ERROR] Cannot find vcvars64.bat.
    echo [HINT] Install Visual Studio Build Tools or set VCVARS to vcvars64.bat.
    exit /b 1
)

if not defined QT_PREFIX (
    if defined CMAKE_PREFIX_PATH set "QT_PREFIX=%CMAKE_PREFIX_PATH%"
)

echo [INFO] Source: %SRC%
echo [INFO] Mirror: %ASCII_SRC%
echo [INFO] Build:  %BUILD%
if defined QT_PREFIX echo [INFO] Qt prefix: %QT_PREFIX%

if exist "%ASCII_SRC%" rmdir /s /q "%ASCII_SRC%"
robocopy "%SRC%" "%ASCII_SRC%" /MIR /XD build .git >nul
if errorlevel 8 (
    echo [ERROR] robocopy failed.
    exit /b 1
)

call "%VCVARS%" || exit /b 1

echo [INFO] Configuring CMake
if defined QT_PREFIX (
    cmake -S "%ASCII_SRC%" -B "%BUILD%" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DCMAKE_CXX_COMPILER=cl -DBACKUP_ALLOW_GTEST_DOWNLOAD=%BACKUP_ALLOW_GTEST_DOWNLOAD% || exit /b 1
) else (
    cmake -S "%ASCII_SRC%" -B "%BUILD%" -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=cl -DBACKUP_ALLOW_GTEST_DOWNLOAD=%BACKUP_ALLOW_GTEST_DOWNLOAD% || exit /b 1
)

echo [INFO] Building
cmake --build "%BUILD%" || exit /b 1

if "%RUN_TESTS%"=="1" (
    echo [INFO] Running tests
    ctest --test-dir "%BUILD%" --output-on-failure || exit /b 1
)

echo [OK] Build completed.
echo [OK] Executable: %BUILD%\BackupTool.exe
endlocal
