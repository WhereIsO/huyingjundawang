@echo off
chcp 65001 >nul
rem 仅演示界面（Mock 后端，不进行真实读写）。
set "PATH=G:\Anaconda3\Library\bin;%PATH%"
set "QT_QPA_PLATFORM_PLUGIN_PATH=G:\Anaconda3\Library\plugins\platforms"
start "" "%~dp0bin\BackupTool.exe"
