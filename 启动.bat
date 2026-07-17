@echo off
REM ======================================================================
REM  Data Backup Tool Launcher
REM  1. Checks Python availability
REM  2. Checks PyQt5 availability, auto-installs if missing
REM  3. Launches GUI
REM ======================================================================

chcp 65001 >nul
cd /d "%~dp0"
echo.

REM --- Check Python ---
python --version >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Python not found. Please install Python 3.8+ and add to PATH.
    echo         https://www.python.org/downloads/
    pause
    exit /b 1
)

REM --- Check PyQt5 ---
python -c "from PyQt5 import QtWidgets" 2>nul
if errorlevel 1 (
    echo [INFO] PyQt5 not found. Installing...
    echo.
    pip install PyQt5>=5.15
    if errorlevel 1 (
        echo.
        echo [ERROR] PyQt5 installation failed.
        echo         Please manually run: pip install PyQt5
        pause
        exit /b 1
    )
    echo.
    echo [OK] PyQt5 installed successfully.
    echo.
)

REM --- Launch GUI ---
echo Starting Data Backup Tool...
echo.
python main.py
if %errorlevel% neq 0 (
    echo.
    echo Program exited with error code: %errorlevel%
    pause
)
