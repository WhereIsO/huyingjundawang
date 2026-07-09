@echo off
chcp 65001 >nul
rem 本地运行脚本：借用本机已安装的 Qt，无需分发库。
rem 需要真实备份/还原功能，因此默认开启 real 后端。
set "BACKUP_BACKEND_MODE=real"
set "PATH=G:\Anaconda3\Library\bin;%PATH%"
set "QT_QPA_PLATFORM_PLUGIN_PATH=G:\Anaconda3\Library\plugins\platforms"
start "" "%~dp0bin\BackupTool.exe"
