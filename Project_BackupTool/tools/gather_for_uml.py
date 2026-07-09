# -*- coding: utf-8 -*-
import sys
sys.stdout.reconfigure(encoding='utf-8')
import os

base = r"F:\Files\Payki in UESTC\学习与课程\VII. Senior I（2026-2027）\【工】软件开发综合实验-李忻洋\Project_BackupTool\gui\BackupTool\src"

# 关键头文件（用于提取类/方法生成 UML）
heads = [
    "BackendAdapter.h", "RealBackend.h", "MainWindow.h",
    "BackupTab.h", "RestoreTab.h", "FilterTab.h", "LogPanel.h", "PathPicker.h",
    os.path.join("core","types.h"),
    os.path.join("core","backup_task.h"),
    os.path.join("core","archive.h"),
    os.path.join("core","scanner.h"),
    os.path.join("core","filter.h"),
    os.path.join("core","huffman.h"),
    os.path.join("core","crypto.h"),
    os.path.join("core","metadata.h"),
]
for h in heads:
    p = os.path.join(base, h)
    print("="*30, h, "="*30)
    if os.path.exists(p):
        print(open(p, encoding="utf-8").read())
    else:
        print("  MISSING")
