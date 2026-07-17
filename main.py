"""Python GUI 入口模块。

本文件是整个图形界面程序的启动入口点。
设计原则：保持入口文件尽量薄，真正的界面创建和事件循环在 app.gui.main() 中完成。
C++ 备份核心由 GUI 作为独立子进程启动，本文件不涉及任何备份业务逻辑。

使用方式：
    python main.py          # 启动图形界面
    pythonw main.py         # 无控制台窗口启动（Windows）

依赖安装：
    pip install PyQt5>=5.15
"""

import sys

# 尝试导入 GUI 模块，如果 PyQt5 不可用则给出友好提示
try:
    from app.gui import main
except ImportError as e:
    print("=" * 60)
    print("错误：无法加载 PyQt5 图形界面库。")
    print()
    print(f"  详细信息：{e}")
    print()
    print(f"  当前 Python 版本：{sys.version}")
    print()
    print("  请先安装 PyQt5：")
    print("    pip install PyQt5>=5.15")
    print("=" * 60)
    sys.exit(1)


# 仅当本文件被直接运行时（而非被 import）才执行以下代码
if __name__ == "__main__":
    # 调用 main() 启动 GUI 事件循环，返回值为退出码（0=正常，非0=异常）
    # 使用 raise SystemExit 而非 sys.exit()，确保退出码能被批处理或自动化脚本正确捕获
    raise SystemExit(main())
