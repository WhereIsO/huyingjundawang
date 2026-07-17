"""数据备份工具的 Python GUI 包。

本包包含图形界面相关的所有 Python 模块：
    - gui.py      : 主窗口、页面、后台任务线程等完整 GUI 实现
    - qt_compat.py: Qt 绑定层（使用 PyQt5）

业务核心（文件扫描、压缩、加密、归档、恢复、校验）全部位于 backend/pbackup_core.cpp，
以独立 C++ 进程运行，本包不包含任何备份算法实现。
"""
