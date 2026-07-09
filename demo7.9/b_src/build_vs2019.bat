@echo off
setlocal enabledelayedexpansion

rem ============================================================
rem  Build script for BackupTool GUI
rem  Works with VS 2019 BuildTools + CMake 3.20
rem ============================================================

rem --- Configuration ---
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "SRC=%~dp0"
if "%SRC:~-1%"=="\" set "SRC=%SRC:~0,-1%"

rem --- Qt5 prefix ---
rem Change this to your Qt5 installation path
set "QT_PREFIX=C:\Qt\5.15.2\msvc2019_64"
if not exist "%QT_PREFIX%" (
    echo [WARN] Qt5 not found at %QT_PREFIX%
    echo [WARN] Please set QT_PREFIX to your Qt5 installation path.
    echo [WARN] Example: set QT_PREFIX=C:\Qt\5.15.2\msvc2019_64
    echo [WARN] Or install Qt5 via: choco install qt5-dev
    echo.
    echo [WARN] Using default: %QT_PREFIX%
)

set "BUILD_DIR=%SRC%\build_vs2019"

if not exist "%VCVARS%" (
    echo [ERROR] Cannot find vcvars64.bat at %VCVARS%
    echo [ERROR] Install Visual Studio 2019 BuildTools or newer.
    exit /b 1
)

echo [1/4] Initializing MSVC environment...
call "%VCVARS%" || exit /b 1

echo [2/4] Configuring CMake...
cmake -S "%SRC%" -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_PREFIX%" ^
    -DCMAKE_CXX_COMPILER=cl ^
    -DBUILD_TESTING=ON || goto :try_msbuild

echo [3/4] Building...
cmake --build "%BUILD_DIR%" || exit /b 1

echo [4/4] Running tests...
ctest --test-dir "%BUILD_DIR%" --output-on-failure || exit /b 1

echo [OK] Build and tests completed.
echo [OK] Executable: %BUILD_DIR%\BackupTool.exe
goto :deploy

:try_msbuild
echo [INFO] Ninja not available, trying Visual Studio generator...
rmdir /s /q "%BUILD_DIR%" 2>nul
cmake -S "%SRC%" -B "%BUILD_DIR%" -G "Visual Studio 16 2019" -A x64 ^
    -DCMAKE_PREFIX_PATH="%QT_PREFIX%" ^
    -DBUILD_TESTING=ON || exit /b 1
cmake --build "%BUILD_DIR%" --config Release || exit /b 1

echo [OK] Build completed with MSBuild.
ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure || exit /b 1

:deploy
echo [INFO] Deploying to bin\ directory...
set "BIN_DIR=%SRC%\..\..\bin"
copy /Y "%BUILD_DIR%\Release\BackupTool.exe" "%BIN_DIR%\BackupTool.exe" >nul 2>nul
copy /Y "%BUILD_DIR%\BackupTool.exe" "%BIN_DIR%\BackupTool.exe" >nul 2>nul
echo [OK] Exe deployed to %BIN_DIR%\BackupTool.exe

endlocal
pause
