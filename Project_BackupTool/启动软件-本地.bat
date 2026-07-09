@echo off
chcp 65001 >nul
title 数据备份工具 - 真实模式
rem 使用项目 bin 目录内自带的 Qt/VC 运行库，不依赖 Anaconda 或固定盘符。
set "BACKUP_BACKEND_MODE=real"
set "APP_DIR=%~dp0bin"
set "PATH=%APP_DIR%;%PATH%"
cd /d "%APP_DIR%"
start "" "%APP_DIR%\BackupTool.exe"
