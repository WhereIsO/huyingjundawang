@echo off
REM ======================================================================
REM  build.bat - PBackup C++ Backend Build Script
REM
REM  Usage:
REM    build.bat         Build DLL and EXE
REM    build.bat clean   Remove build artifacts
REM
REM  Requirements:
REM    - MinGW-w64 GCC 10+ (recommended 16.x) in PATH or set CXX env var
REM    - C++17 standard support
REM    - Windows 7+ (declared via _WIN32_WINNT=0x0601)
REM
REM  Output:
REM    pbackup_core.dll    - Core dynamic library (all backup logic)
REM    pbackup_core.exe    - Lightweight launcher (loads DLL at runtime)
REM    libpbackup_core.a   - DLL import library (for link-time use)
REM ======================================================================
setlocal

REM Switch to script directory for correct relative paths
cd /d "%~dp0"

REM Handle "clean" argument
if "%1"=="clean" goto do_clean

REM --- Compiler lookup ---
REM Prefer user-specified CXX environment variable
if defined CXX goto compiler_ready

REM Otherwise search PATH for g++
where g++ >nul 2>nul
if errorlevel 1 goto compiler_missing
set "CXX=g++"

:compiler_ready
echo Building PBackup C++ core (DLL + EXE)...
echo.

REM --- Step 1: Build core dynamic library ---
REM Flags:
REM   -std=c++17              C++17 standard (filesystem, optional, etc.)
REM   -O2                     Optimization level 2
REM   -Wall -Wextra           Enable most warnings
REM   -Wno-cast-function-type Suppress GetProcAddress cast warnings
REM   -DPBACKUP_BUILDING_DLL  Expands PBACKUP_API to __declspec(dllexport)
REM   -shared                 Output as DLL
REM   -static-libgcc/stdc++   Static link runtime (no extra DLL needed)
REM   -lbcrypt                Link Windows CNG crypto library
REM   -Wl,--out-implib,...    Generate import library for other programs
echo [1/2] Building pbackup_core.dll ...
"%CXX%" -std=c++17 -O2 -Wall -Wextra -Wno-cast-function-type ^
    -D_WIN32_WINNT=0x0601 -DPBACKUP_BUILDING_DLL ^
    -shared -o pbackup_core.dll pbackup_core.cpp ^
    -static-libgcc -static-libstdc++ ^
    -lbcrypt -Wl,--out-implib,libpbackup_core.a
if errorlevel 1 goto build_failed_dll
echo       OK: %CD%\pbackup_core.dll
echo.

REM --- Step 2: Build launcher executable ---
REM Flags:
REM   -municode               Use wmain entry point (Unicode argv support)
REM   No -lbcrypt             Launcher only forwards to DLL, no crypto calls
echo [2/2] Building pbackup_core.exe ...
"%CXX%" -std=c++17 -O2 -Wall -Wextra ^
    -D_WIN32_WINNT=0x0601 -municode ^
    pbackup_main.cpp -o pbackup_core.exe ^
    -static-libgcc -static-libstdc++
if errorlevel 1 goto build_failed_exe
echo       OK: %CD%\pbackup_core.exe
echo.

echo Build complete.
echo   DLL: pbackup_core.dll
echo   EXE: pbackup_core.exe
exit /b 0

REM --- Clean build artifacts ---
:do_clean
echo Cleaning...
if exist pbackup_core.dll del pbackup_core.dll
if exist pbackup_core.exe del pbackup_core.exe
if exist libpbackup_core.a del libpbackup_core.a
echo Done.
exit /b 0

REM --- Error handlers ---
:compiler_missing
echo ERROR: g++ not found. Install MinGW-w64 (GCC 10+) or set CXX environment variable.
exit /b 1

:build_failed_dll
echo ERROR: pbackup_core.dll build failed. GCC 10+ with C++17 filesystem support required.
exit /b 1

:build_failed_exe
echo ERROR: pbackup_core.exe build failed.
exit /b 1
