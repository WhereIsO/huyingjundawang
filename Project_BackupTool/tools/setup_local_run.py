# -*- coding: utf-8 -*-
from pathlib import Path
import sys

sys.stdout.reconfigure(encoding="utf-8")

root = Path(__file__).resolve().parents[1]
bin_dir = root / "bin"
exe = bin_dir / "BackupTool.exe"

if not exe.exists():
    raise SystemExit(f"未找到 {exe}，请先构建或解压 dist/数据备份工具_可分发.zip")

real_bat = root / "启动软件-本地.bat"
mock_bat = root / "启动软件-仅演示界面.bat"

real_bat.write_text(
    "@echo off\r\n"
    "chcp 65001 >nul\r\n"
    "title 数据备份工具 - 真实模式\r\n"
    "rem 使用项目 bin 目录内自带的 Qt/VC 运行库，不依赖 Anaconda 或固定盘符。\r\n"
    "set \"BACKUP_BACKEND_MODE=real\"\r\n"
    "set \"APP_DIR=%~dp0bin\"\r\n"
    "set \"PATH=%APP_DIR%;%PATH%\"\r\n"
    "cd /d \"%APP_DIR%\"\r\n"
    "start \"\" \"%APP_DIR%\\BackupTool.exe\"\r\n",
    encoding="utf-8",
)

mock_bat.write_text(
    "@echo off\r\n"
    "chcp 65001 >nul\r\n"
    "title 数据备份工具 - 演示模式\r\n"
    "rem 演示模式不设置 BACKUP_BACKEND_MODE，后端使用 Mock，不执行真实备份/还原。\r\n"
    "set \"BACKUP_BACKEND_MODE=\"\r\n"
    "set \"APP_DIR=%~dp0bin\"\r\n"
    "set \"PATH=%APP_DIR%;%PATH%\"\r\n"
    "cd /d \"%APP_DIR%\"\r\n"
    "start \"\" \"%APP_DIR%\\BackupTool.exe\"\r\n",
    encoding="utf-8",
)

print("EXE:", exe)
print("Wrote:", real_bat)
print("Wrote:", mock_bat)
