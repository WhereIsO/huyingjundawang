@echo off
chcp 65001 >nul
title 数据备份工具 - 演示模式
rem 演示模式不设置 BACKUP_BACKEND_MODE，后端使用 Mock，不执行真实备份/还原。
set "BACKUP_BACKEND_MODE="
set "APP_DIR=%~dp0bin"
set "PATH=%APP_DIR%;%PATH%"
cd /d "%APP_DIR%"
start "" "%APP_DIR%\BackupTool.exe"
