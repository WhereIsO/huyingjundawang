# -*- coding: utf-8 -*-
import sys
sys.stdout.reconfigure(encoding='utf-8')
import os

# 本地已构建的 exe
for p in [r"G:\_build_backup_tool_gui_ascii\BackupTool.exe",
          r"G:\_bmsvc\BackupTool.exe"]:
    print("EXE", os.path.exists(p), p)

# Qt 运行库位置
qtbin = r"G:\Anaconda3\Library\bin"
print("Qt bin exists:", os.path.exists(qtbin))
plat = r"G:\Anaconda3\Library\plugins\platforms\qwindows.dll"
print("qwindows plugin:", os.path.exists(plat))

# 课程文件夹 gui 目录，用于放本地启动脚本
gui = r"F:\Files\Payki in UESTC\学习与课程\VII. Senior I（2026-2027）\【工】软件开发综合实验-李忻洋\Project_BackupTool\gui\BackupTool"
print("gui dir exists:", os.path.exists(gui))
print("build_msvc.bat exists:", os.path.exists(os.path.join(gui,"build_msvc.bat")))
