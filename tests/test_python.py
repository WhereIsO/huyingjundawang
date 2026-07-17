"""Python 层单元测试。

运行方式：
    python -m pytest tests/test_python.py -v
"""

import sys
from pathlib import Path

# 确保项目根目录在 sys.path 中
sys.path.insert(0, str(Path(__file__).parent.parent))


class TestSplitTerms:
    """测试 split_terms 输入分割。"""

    def test_comma_separated(self):
        from app.gui import split_terms
        assert split_terms("a, b, c") == ["a", "b", "c"]

    def test_chinese_comma(self):
        from app.gui import split_terms
        assert split_terms("文件夹1，文件夹2") == ["文件夹1", "文件夹2"]

    def test_semicolon(self):
        from app.gui import split_terms
        assert split_terms("a;b;c") == ["a", "b", "c"]

    def test_newline(self):
        from app.gui import split_terms
        assert split_terms("a\nb\nc") == ["a", "b", "c"]

    def test_empty_string(self):
        from app.gui import split_terms
        assert split_terms("") == []

    def test_whitespace_only(self):
        from app.gui import split_terms
        assert split_terms("  ,  , ") == []

    def test_mixed_separators(self):
        from app.gui import split_terms
        assert split_terms("a，b;c\nd") == ["a", "b", "c", "d"]


class TestEscapeHtml:
    """测试 escape_html 转义。"""

    def test_ampersand(self):
        from app.gui import escape_html
        assert escape_html("a & b") == "a &amp; b"

    def test_angle_brackets(self):
        from app.gui import escape_html
        assert escape_html("<script>") == "&lt;script&gt;"

    def test_no_change(self):
        from app.gui import escape_html
        assert escape_html("normal text") == "normal text"

    def test_all_special(self):
        from app.gui import escape_html
        assert escape_html("<>&") == "&lt;&gt;&amp;"


class TestQtCompat:
    """测试 Qt 兼容层导入。"""

    def test_binding_name(self):
        from app.qt_compat import BINDING_NAME
        assert BINDING_NAME in ("PyQt5", "PySide6", "PySide2")

    def test_core_modules(self):
        from app.qt_compat import QtCore, QtGui, QtWidgets
        assert QtCore is not None
        assert QtGui is not None
        assert QtWidgets is not None

    def test_signal_importable(self):
        from app.qt_compat import Signal
        assert Signal is not None


class TestProjectPaths:
    """测试项目路径常量。"""

    def test_backend_path_exists(self):
        from app.gui import BACKEND_PATH
        assert BACKEND_PATH.name == "pbackup_core.exe"

    def test_background_path_exists(self):
        from app.gui import BACKGROUND_PATH
        assert BACKGROUND_PATH.suffix == ".jpg"

    def test_project_root(self):
        from app.gui import PROJECT_ROOT
        assert (PROJECT_ROOT / "main.py").exists()
