"""Qt 绑定兼容层模块。

本项目使用 PyQt5 作为 Qt 绑定。
安装方式：pip install PyQt5>=5.15

导出内容：
    - QtCore     : Qt 核心模块（信号槽、事件循环、定时器等）
    - QtGui      : Qt 图形模块（字体、颜色、图片、绘图等）
    - QtWidgets  : Qt 控件模块（按钮、输入框、窗口等）
    - Signal     : PyQt5 信号声明类
    - BINDING_NAME : 绑定名称字符串 "PyQt5"
    - exec_app() : 事件循环启动函数
"""

from __future__ import annotations

# 导入 PyQt5 核心模块
from PyQt5 import QtCore, QtGui, QtWidgets
# PyQt5 中信号的声明类名为 pyqtSignal，重命名为 Signal 以简化使用
from PyQt5.QtCore import pyqtSignal as Signal

# 当前使用的 Qt 绑定名称，供日志和调试信息使用
BINDING_NAME = "PyQt5"


def exec_app(app: "QtWidgets.QApplication") -> int:
    """启动 Qt 事件循环。

    参数:
        app: QApplication 实例，已完成窗口创建和显示

    返回:
        int: 事件循环退出码（0=正常退出，非0=异常退出）
    """
    if hasattr(app, "exec"):
        return int(app.exec())
    return int(app.exec_())
