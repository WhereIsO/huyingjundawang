@echo off
chcp 65001 >nul
title 数据备份工具
set "BACKUP_BACKEND_MODE=real"
"%~dp0bin\BackupTool.exe"