# -*- coding: utf-8 -*-
import os, shutil, sys

# 与 final_check.py 相同的、已验证可用的课程路径
proj = r"F:\Files\Payki in UESTC\学习与课程\VII. Senior I（2026-2027）\【工】软件开发综合实验-李忻洋\Project_BackupTool"

# 候选 exe（取最新构建）
candidates = [
    r"G:\_build_backup_tool_gui_ascii\BackupTool.exe",
    r"G:\_bmsvc\BackupTool.exe",
]
src_exe = None
for c in candidates:
    if os.path.isfile(c):
        if src_exe is None or os.path.getmtime(c) > os.path.getmtime(src_exe):
            src_exe = c

print("proj exists:", os.path.isdir(proj))
print("src exe:", src_exe)

if not os.path.isdir(proj):
    sys.exit("课程目录不存在，路径可能有误")
if not src_exe:
    sys.exit("未找到已构建的 exe")

# 1) 复制 exe 到课程文件夹 bin\
bin_dir = os.path.join(proj, "bin")
os.makedirs(bin_dir, exist_ok=True)
dst_exe = os.path.join(bin_dir, "BackupTool.exe")
shutil.copy2(src_exe, dst_exe)
print("copied exe ->", dst_exe, round(os.path.getsize(dst_exe)/1024/1024, 2), "MB")

# 2) 本地启动脚本：借用本机已装的 Anaconda Qt，无需打包库
qt_bin = r"G:\Anaconda3\Library\bin"
qt_plat = r"G:\Anaconda3\Library\plugins\platforms"

bat = (
    "@echo off\r\n"
    "chcp 65001 >nul\r\n"
    "rem 本地运行脚本：借用本机已安装的 Qt，无需分发库。\r\n"
    "rem 需要真实备份/还原功能，因此默认开启 real 后端。\r\n"
    "set \"BACKUP_BACKEND_MODE=real\"\r\n"
    "set \"PATH=" + qt_bin + ";%PATH%\"\r\n"
    "set \"QT_QPA_PLATFORM_PLUGIN_PATH=" + qt_plat + "\"\r\n"
    "start \"\" \"%~dp0bin\\BackupTool.exe\"\r\n"
)
bat_path = os.path.join(proj, "启动软件-本地.bat")
with open(bat_path, "w", encoding="utf-8") as f:
    f.write(bat)
print("wrote:", bat_path)

# 3) 演示用后端切换脚本（Mock，仅看界面不落盘）
bat_mock = (
    "@echo off\r\n"
    "chcp 65001 >nul\r\n"
    "rem 仅演示界面（Mock 后端，不进行真实读写）。\r\n"
    "set \"PATH=" + qt_bin + ";%PATH%\"\r\n"
    "set \"QT_QPA_PLATFORM_PLUGIN_PATH=" + qt_plat + "\"\r\n"
    "start \"\" \"%~dp0bin\\BackupTool.exe\"\r\n"
)
bat_mock_path = os.path.join(proj, "启动软件-仅演示界面.bat")
with open(bat_mock_path, "w", encoding="utf-8") as f:
    f.write(bat_mock)
print("wrote:", bat_mock_path)

print("DONE")
