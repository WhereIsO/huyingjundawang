# -*- coding: utf-8 -*-
import os
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

root = Path(__file__).resolve().parents[1]
bin_dir = root / "bin"
exe = bin_dir / "BackupTool.exe"
real_launcher = root / "启动软件-本地.bat"
mock_launcher = root / "启动软件-仅演示界面.bat"
dist_zip = root / "dist" / "数据备份工具_可分发.zip"

required_runtime = [
    "Qt5Core_conda.dll",
    "Qt5Gui_conda.dll",
    "Qt5Widgets_conda.dll",
    "platforms/qwindows.dll",
    "VCRUNTIME140.dll",
]

print("Project root:", root)
print("EXE:", exe.exists(), exe)
print("Real launcher:", real_launcher.exists(), real_launcher)
print("Mock launcher:", mock_launcher.exists(), mock_launcher)
print("Dist zip:", dist_zip.exists(), dist_zip)
print("Runtime files:")
for name in required_runtime:
    p = bin_dir / name
    print(" ", p.exists(), name)
