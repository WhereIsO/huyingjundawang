@echo off
chcp 65001 >nul
title 数据备份工具
rem 让程序调用真实备份/还原后端（不设置则只是界面演示）
set "BACKUP_BACKEND_MODE=real"
rem Qt 运行库和平台插件都在本文件夹内，无需额外安装
cd /d "%~dp0"
start "" "%~dp0BackupTool.exe"
