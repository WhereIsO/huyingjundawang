@echo off
chcp 65001 >nul
set "BACKUP_BACKEND_MODE=mock"
"%~dp0bin\BackupTool.exe"