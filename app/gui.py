"""PyQt 图形界面模块。

本模块实现 PBackup 数据备份工具的完整图形用户界面。

架构职责划分：
    - Python GUI (本模块): 窗口布局、输入校验、进度展示、用户交互、背景个性化
    - C++ 核心 (pbackup_core.exe): 文件扫描、筛选、压缩、加密、归档、恢复、校验

通信机制：
    - GUI → C++ : 通过 subprocess 启动子进程，命令行参数传递配置，环境变量传递密码
    - C++ → GUI : 通过 stdout 输出 JSON Lines 事件（progress/log/result/error）
    - GUI → C++ 取消: 向子进程 stdin 写入 "cancel\\n"

界面结构：
    ┌────────────────────────────────────────────────────────┐
    │  BackgroundWidget (自定义背景图片绘制)                    │
    │  ┌──────────┐  ┌──────────────────────────────────────┐│
    │  │ Sidebar  │  │ ContentShell                         ││
    │  │ - 创建备份│  │ ┌────────────────────────────────┐  ││
    │  │ - 恢复数据│  │ │ QStackedWidget (页面切换)       │  ││
    │  │ - 运行日志│  │ │  - backup_page (备份参数)      │  ││
    │  │          │  │ │  - restore_page (恢复参数)     │  ││
    │  │ 背景设置  │  │ │  - log_page (运行日志)         │  ││
    │  └──────────┘  │ └────────────────────────────────┘  ││
    │                │ ┌────────────────────────────────┐  ││
    │                │ │ ProgressFooter (阶段+进度条)    │  ││
    │                │ └────────────────────────────────┘  ││
    │                └──────────────────────────────────────┘│
    └────────────────────────────────────────────────────────┘
"""

from __future__ import annotations

import datetime as dt       # 日期时间处理，用于日志时间戳
import json                 # JSON 解析，用于解析 C++ 后端的事件输出
import os                   # 操作系统接口，用于环境变量和路径操作
import subprocess           # 子进程管理，用于启动 C++ 后端
import sys                  # 系统参数，用于获取命令行参数和退出
from pathlib import Path    # 面向对象的路径操作
from typing import Any      # 类型注解支持

from .qt_compat import BINDING_NAME, QtCore, QtGui, QtWidgets, Signal, exec_app


# 通过当前源码位置推导项目根目录，不写死任何电脑上的绝对路径。
# 通过当前源码文件位置自动推导项目根目录（避免硬编码绝对路径）
PROJECT_ROOT = Path(__file__).parent.parent
# 默认背景图片路径（项目根目录下的 background.jpg）
BACKGROUND_PATH = PROJECT_ROOT / "background.jpg"
# C++ 后端可执行文件路径（相对于项目根目录）
BACKEND_PATH = PROJECT_ROOT / "backend" / "pbackup_core.exe"


class GuiError(RuntimeError):
    """界面输入或 C++ 后端调用错误。"""


def split_terms(text: str) -> list[str]:
    """把界面中的逗号、分号和换行分隔文本拆成参数。"""

    normalized = text.replace("，", ",").replace("；", ";").replace("\n", ",").replace(";", ",")
    return [part.strip() for part in normalized.split(",") if part.strip()]


def backend_request(arguments: list[str], password: str = "", timeout: int = 30) -> dict[str, Any]:
    """同步调用轻量 C++ 命令，用于读取能力列表和包头。"""

    if not BACKEND_PATH.is_file():
        raise GuiError(f"找不到 C++ 核心：{BACKEND_PATH}。请先运行 backend\\build.bat。")
    environment = os.environ.copy()
    environment["PBACKUP_PASSWORD"] = password
    # Windows 下创建无窗口进程，避免弹出控制台黑窗
    creation_flags = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
    # 同步执行 C++ 后端命令，捕获标准输出和标准错误
    completed = subprocess.run(
        [str(BACKEND_PATH), *arguments],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=environment,
        timeout=timeout,
        creationflags=creation_flags,
        check=False,
    )
    result: dict[str, Any] | None = None
    error_message = ""
    for raw_line in completed.stdout.splitlines():
        try:
            event = json.loads(raw_line)
        except json.JSONDecodeError:
            continue
        if event.get("type") == "result" and isinstance(event.get("data"), dict):
            result = event["data"]
        elif event.get("type") == "error":
            error_message = str(event.get("message", "C++ 核心执行失败。"))
    if error_message:
        raise GuiError(error_message)
    if completed.returncode != 0 or result is None:
        details = completed.stderr.strip() or completed.stdout.strip() or f"退出代码 {completed.returncode}"
        raise GuiError(f"C++ 核心未返回有效结果：{details}")
    return result


class BackgroundWidget(QtWidgets.QWidget):
    """绘制背景图片和遮罩的根控件。

    使用 paintEvent 绘制而不是写死 QSS url，可以确保程序从不同工作目录启动时
    仍能找到项目中的 background.jpg。
    """

    def __init__(self, background_path: Path, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self._pixmap = QtGui.QPixmap()
        self._background_path = Path()
        if not self.set_background(background_path) and background_path != BACKGROUND_PATH:
            self.set_background(BACKGROUND_PATH)
        self.setObjectName("BackgroundRoot")

    def set_background(self, background_path: Path) -> bool:
        """加载并立即应用背景图片，返回图片是否有效。"""

        pixmap = QtGui.QPixmap(str(background_path))
        if pixmap.isNull():
            return False
        self._pixmap = pixmap
        self._background_path = background_path
        self.update()
        return True

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:  # noqa: N802 - Qt 虚函数命名保持框架风格。
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.SmoothPixmapTransform, True)
        rect = self.rect()

        if not self._pixmap.isNull():
            scaled = self._pixmap.scaled(rect.size(), QtCore.Qt.KeepAspectRatioByExpanding, QtCore.Qt.SmoothTransformation)
            x = (rect.width() - scaled.width()) // 2
            y = (rect.height() - scaled.height()) // 2
            painter.drawPixmap(x, y, scaled)
        else:
            painter.fillRect(rect, QtGui.QColor("#e8f5e9"))

        # 不使用遮罩，让背景图原生展示。
        super().paintEvent(event)


class PathRow(QtWidgets.QWidget):
    """路径选择行，统一处理备份源、目录、打开文件和保存文件场景。"""

    def __init__(self, title: str, mode: str, placeholder: str, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self._mode = mode
        self._title = title

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(10)

        label = QtWidgets.QLabel(title)
        label.setObjectName("FieldLabel")
        label.setFixedWidth(88)
        self.edit = QtWidgets.QLineEdit()
        self.edit.setPlaceholderText(placeholder)
        self.edit.setClearButtonEnabled(True)
        self.button = QtWidgets.QPushButton("浏览")
        self.button.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_DialogOpenButton))
        self.button.clicked.connect(self._browse)

        layout.addWidget(label)
        layout.addWidget(self.edit, 1)
        layout.addWidget(self.button)
        self.directory_button: QtWidgets.QPushButton | None = None
        if self._mode == "source":
            self.button.setText("选文件")
            self.directory_button = QtWidgets.QPushButton("选文件夹")
            self.directory_button.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_DirOpenIcon))
            self.directory_button.clicked.connect(self._browse_source_directory)
            layout.addWidget(self.directory_button)

    def text(self) -> str:
        """返回用户输入的路径文本。"""

        return self.edit.text().strip()

    def set_text(self, value: str) -> None:
        """设置路径文本，供读取包头等操作回填。"""

        self.edit.setText(value)

    def _browse(self) -> None:
        """根据模式弹出对应 QFileDialog。"""

        current = self.text() or str(PROJECT_ROOT)
        if self._mode == "source":
            selected, _ = QtWidgets.QFileDialog.getOpenFileName(self, "选择要备份的文件", current, "所有文件 (*)")
        elif self._mode == "directory":
            selected = QtWidgets.QFileDialog.getExistingDirectory(self, f"选择{self._title}", current)
        elif self._mode == "save":
            selected, _ = QtWidgets.QFileDialog.getSaveFileName(self, f"选择{self._title}", current, "备份包 (*.pbackup);;所有文件 (*)")
            if selected and not selected.lower().endswith(".pbackup"):
                selected += ".pbackup"
        else:
            selected, _ = QtWidgets.QFileDialog.getOpenFileName(self, f"选择{self._title}", current, "备份包 (*.pbackup);;所有文件 (*)")
        if selected:
            self.edit.setText(selected)

    def _browse_source_directory(self) -> None:
        """为备份源选择一个文件夹。"""

        current = self.text() or str(PROJECT_ROOT)
        if Path(current).is_file():
            current = str(Path(current).parent)
        selected = QtWidgets.QFileDialog.getExistingDirectory(self, "选择要备份的文件夹", current)
        if selected:
            self.edit.setText(selected)


class OperationWorker(QtCore.QThread):
    """启动 C++ 后端并把逐行 JSON 事件转成 Qt 信号。"""

    # Qt 信号：进度更新时发射，携带进度字典（stage, total_files, done_bytes 等）
    progress_changed = Signal(object)
    # Qt 信号：日志消息，参数为 (级别, 消息文本)
    log_emitted = Signal(str, str)
    # Qt 信号：任务成功完成，携带结果数据
    succeeded = Signal(object)
    # Qt 信号：任务失败，携带错误消息字符串
    failed = Signal(str)

    def __init__(self, payload: dict[str, Any], parent: QtCore.QObject | None = None) -> None:
        super().__init__(parent)
        self.payload = payload
        self.process: subprocess.Popen[str] | None = None

    def cancel(self) -> None:
        """通过标准输入通知 C++ 后端安全取消。"""

        process = self.process
        if process is None or process.poll() is not None or process.stdin is None:
            return
        try:
            process.stdin.write("cancel\n")
            process.stdin.flush()
        except (BrokenPipeError, OSError):
            pass

    def run(self) -> None:  # noqa: D401 - Qt 线程入口。
        """执行 C++ 子进程并转发进度、日志、结果与错误。"""

        try:
            if not BACKEND_PATH.is_file():
                raise GuiError(f"找不到 C++ 核心：{BACKEND_PATH}。请先运行 backend\\build.bat。")
            environment = os.environ.copy()
            environment["PBACKUP_PASSWORD"] = str(self.payload.get("password", ""))
            creation_flags = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
            self.process = subprocess.Popen(
                [str(BACKEND_PATH), *self.payload["arguments"]],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                errors="replace",
                env=environment,
                creationflags=creation_flags,
                bufsize=1,
            )
            result: object | None = None
            error_message = ""
            assert self.process.stdout is not None
            for raw_line in self.process.stdout:
                line = raw_line.strip()
                if not line:
                    continue
                try:
                    event = json.loads(line)
                except json.JSONDecodeError:
                    self.log_emitted.emit("warn", f"C++ 核心输出了无法解析的消息：{line}")
                    continue
                event_type = event.get("type")
                if event_type == "progress":
                    self.progress_changed.emit(event)
                elif event_type == "log":
                    self.log_emitted.emit(str(event.get("level", "info")), str(event.get("message", "")))
                elif event_type == "result":
                    result = event.get("data")
                elif event_type == "error":
                    error_message = str(event.get("message", "C++ 核心执行失败。"))

            stderr = self.process.stderr.read().strip() if self.process.stderr is not None else ""
            return_code = self.process.wait()
            if error_message:
                self.failed.emit(error_message)
            elif return_code != 0:
                self.failed.emit(stderr or f"C++ 核心异常退出，错误代码：{return_code}")
            elif result is None:
                self.failed.emit(stderr or "C++ 核心未返回任务结果。")
            else:
                self.succeeded.emit(result)
        except Exception as exc:  # noqa: BLE001 - GUI 边界统一兜底，避免线程异常直接吞掉。
            self.failed.emit(f"任务异常：{exc}")
        finally:
            self.process = None


class MainWindow(QtWidgets.QMainWindow):
    """主窗口，包含备份、恢复和日志三个工作页。"""

    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("数据备份工具 - Python GUI / C++ Core")
        self.resize(1180, 760)
        self.setMinimumSize(980, 640)
        self.current_worker: OperationWorker | None = None  # 当前后台任务线程引用
        self.settings = QtCore.QSettings("UESTC", "PBackupTool")  # 持久化用户设置（背景图片等）
        saved_background = str(self.settings.value("appearance/background_path", "") or "")
        saved_background_path = Path(saved_background) if saved_background else None
        if saved_background_path is not None and saved_background_path.is_file():
            self.background_path = saved_background_path
        else:
            self.background_path = BACKGROUND_PATH
            if saved_background:
                self.settings.remove("appearance/background_path")
        try:
            self.backend_capabilities = backend_request(["capabilities"])
            self.backend_startup_error = ""
        except GuiError as exc:
            self.backend_capabilities = {
                "compression": {"stored": "无压缩"},
                "encryption": {"none": "不加密"},
            }
            self.backend_startup_error = str(exc)

        self._build_ui()
        self._apply_style()
        self._log("info", f"Python GUI 启动完成，当前 Qt 绑定：{BINDING_NAME}。")
        if self.backend_startup_error:
            self._log("error", self.backend_startup_error)
            QtCore.QTimer.singleShot(0, lambda: self._show_error(self.backend_startup_error))
        else:
            self._log("ok", "C++ 备份核心已连接。")

    def _build_ui(self) -> None:
        """创建主界面结构。"""

        self.background_widget = BackgroundWidget(self.background_path)
        self.setCentralWidget(self.background_widget)
        outer = QtWidgets.QHBoxLayout(self.background_widget)
        outer.setContentsMargins(22, 22, 22, 22)
        outer.setSpacing(18)

        self.sidebar = self._build_sidebar()
        outer.addWidget(self.sidebar)

        content = QtWidgets.QFrame()
        content.setObjectName("ContentShell")
        content_layout = QtWidgets.QVBoxLayout(content)
        content_layout.setContentsMargins(24, 22, 24, 22)
        content_layout.setSpacing(14)

        self.stack = QtWidgets.QStackedWidget()
        self.backup_page = self._build_backup_page()
        self.restore_page = self._build_restore_page()
        self.log_page = self._build_log_page()
        self.stack.addWidget(self.backup_page)
        self.stack.addWidget(self.restore_page)
        self.stack.addWidget(self.log_page)

        content_layout.addWidget(self.stack, 1)
        content_layout.addWidget(self._build_progress_footer())
        outer.addWidget(content, 1)

    def _build_sidebar(self) -> QtWidgets.QFrame:
        """创建左侧导航栏。"""

        sidebar = QtWidgets.QFrame()
        sidebar.setObjectName("Sidebar")
        sidebar.setFixedWidth(228)
        layout = QtWidgets.QVBoxLayout(sidebar)
        layout.setContentsMargins(18, 20, 18, 18)
        layout.setSpacing(12)

        title = QtWidgets.QLabel("数据备份工具")
        title.setObjectName("AppTitle")
        subtitle = QtWidgets.QLabel("Python GUI · C++ Core")
        subtitle.setObjectName("AppSubtitle")
        layout.addWidget(title)
        layout.addWidget(subtitle)
        layout.addSpacing(14)

        self.nav_buttons: list[QtWidgets.QPushButton] = []
        for index, text in enumerate(["创建备份", "恢复数据", "运行日志"]):
            button = QtWidgets.QPushButton(text)
            button.setCheckable(True)
            button.setCursor(QtCore.Qt.PointingHandCursor)
            button.clicked.connect(lambda checked=False, page=index: self._switch_page(page))
            self.nav_buttons.append(button)
            layout.addWidget(button)
        self.nav_buttons[0].setChecked(True)

        layout.addStretch(1)
        self.change_background_button = QtWidgets.QPushButton("选择背景图片")
        self.change_background_button.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_FileDialogContentsView))
        self.change_background_button.setToolTip("选择 JPG、PNG、BMP 或 WebP 图片作为界面背景")
        self.change_background_button.clicked.connect(self._choose_background)
        self.reset_background_button = QtWidgets.QPushButton("恢复默认背景")
        self.reset_background_button.clicked.connect(self._reset_background)
        layout.addWidget(self.change_background_button)
        layout.addWidget(self.reset_background_button)
        info = QtWidgets.QLabel("压缩：MSZIP / XPRESS / LZMS\n加密：AES-GCM / ChaCha20")
        info.setObjectName("SidebarInfo")
        info.setWordWrap(True)
        layout.addWidget(info)
        return sidebar

    def _build_backup_page(self) -> QtWidgets.QWidget:
        """创建备份页。"""

        page, body = self._page("创建备份", "选择单个文件或文件夹，并设置压缩、加密和筛选条件。")

        basic = self._panel("备份位置")
        basic_layout = basic.layout()
        basic_layout.setSpacing(12)
        self.backup_source = PathRow("备份源", "source", "选择要备份的单个文件或文件夹")
        self.backup_output = PathRow("备份包", "save", "保存为 .pbackup")
        basic_layout.addWidget(self.backup_source)
        basic_layout.addWidget(self.backup_output)
        body.addWidget(basic)

        options = self._panel("压缩与加密")
        options_layout = QtWidgets.QGridLayout()
        options_layout.setHorizontalSpacing(14)
        options_layout.setVerticalSpacing(12)

        self.compression_combo = QtWidgets.QComboBox()
        for key, label in dict(self.backend_capabilities.get("compression", {})).items():
            self.compression_combo.addItem(label, key)
        default_compression = "mszip" if self.compression_combo.findData("mszip") >= 0 else "stored"
        self._set_combo_value(self.compression_combo, default_compression)

        self.encryption_combo = QtWidgets.QComboBox()
        for key, label in dict(self.backend_capabilities.get("encryption", {})).items():
            self.encryption_combo.addItem(label, key)
        self.encryption_combo.currentIndexChanged.connect(self._sync_password_state)

        self.password_edit = QtWidgets.QLineEdit()
        self.password_edit.setEchoMode(QtWidgets.QLineEdit.Password)
        self.password_edit.setPlaceholderText("选择加密后填写")
        self.password_confirm_edit = QtWidgets.QLineEdit()
        self.password_confirm_edit.setEchoMode(QtWidgets.QLineEdit.Password)
        self.password_confirm_edit.setPlaceholderText("再次输入密码")
        self.kdf_spin = QtWidgets.QSpinBox()
        self.kdf_spin.setRange(100_000, 2_000_000)
        self.kdf_spin.setSingleStep(50_000)
        self.kdf_spin.setValue(600_000)
        self.kdf_spin.setSuffix(" 次")

        options_layout.addWidget(self._label("压缩方式"), 0, 0)
        options_layout.addWidget(self.compression_combo, 0, 1)
        options_layout.addWidget(self._label("加密方式"), 0, 2)
        options_layout.addWidget(self.encryption_combo, 0, 3)
        options_layout.addWidget(self._label("密码"), 1, 0)
        options_layout.addWidget(self.password_edit, 1, 1)
        options_layout.addWidget(self._label("确认密码"), 1, 2)
        options_layout.addWidget(self.password_confirm_edit, 1, 3)
        options_layout.addWidget(self._label("KDF 迭代"), 2, 0)
        options_layout.addWidget(self.kdf_spin, 2, 1)
        options.layout().addLayout(options_layout)
        body.addWidget(options)

        filters = self._panel("筛选条件")
        filters_layout = QtWidgets.QGridLayout()
        filters_layout.setHorizontalSpacing(14)
        filters_layout.setVerticalSpacing(12)
        self.include_text = QtWidgets.QTextEdit()
        self.include_text.setFixedHeight(70)
        self.include_text.setPlaceholderText("包含路径关键词，逗号或换行分隔")
        self.exclude_text = QtWidgets.QTextEdit()
        self.exclude_text.setFixedHeight(70)
        self.exclude_text.setPlaceholderText("排除路径关键词，逗号或换行分隔")
        self.name_glob = QtWidgets.QLineEdit()
        self.name_glob.setPlaceholderText("例如 *.docx 或 report_??.xlsx")

        self.type_file = QtWidgets.QCheckBox("文件")
        self.type_dir = QtWidgets.QCheckBox("目录")
        self.type_symlink = QtWidgets.QCheckBox("符号链接")
        self.type_file.setChecked(True)
        self.type_dir.setChecked(True)
        type_row = QtWidgets.QHBoxLayout()
        type_row.addWidget(self.type_file)
        type_row.addWidget(self.type_dir)
        type_row.addWidget(self.type_symlink)
        type_row.addStretch(1)

        self.min_size_spin = QtWidgets.QSpinBox()
        self.min_size_spin.setRange(0, 999_999_999)
        self.min_size_spin.setSpecialValueText("不限")
        self.min_size_unit = QtWidgets.QComboBox()
        self.min_size_unit.addItem("B", 1)
        self.min_size_unit.addItem("KB", 1024)
        self.min_size_unit.addItem("MB", 1024 * 1024)
        self.min_size_unit.addItem("GB", 1024 * 1024 * 1024)
        self.min_size_row = QtWidgets.QHBoxLayout()
        self.min_size_row.setSpacing(6)
        self.min_size_row.addWidget(self.min_size_spin, 1)
        self.min_size_row.addWidget(self.min_size_unit)

        self.max_size_spin = QtWidgets.QSpinBox()
        self.max_size_spin.setRange(0, 999_999_999)
        self.max_size_spin.setSpecialValueText("不限")
        self.max_size_unit = QtWidgets.QComboBox()
        self.max_size_unit.addItem("B", 1)
        self.max_size_unit.addItem("KB", 1024)
        self.max_size_unit.addItem("MB", 1024 * 1024)
        self.max_size_unit.addItem("GB", 1024 * 1024 * 1024)
        self.max_size_row = QtWidgets.QHBoxLayout()
        self.max_size_row.setSpacing(6)
        self.max_size_row.addWidget(self.max_size_spin, 1)
        self.max_size_row.addWidget(self.max_size_unit)

        self.after_check = QtWidgets.QCheckBox("修改时间晚于")
        self.after_date = QtWidgets.QDateEdit(QtCore.QDate.currentDate().addMonths(-1))
        self.after_date.setCalendarPopup(True)
        self.before_check = QtWidgets.QCheckBox("修改时间早于")
        self.before_date = QtWidgets.QDateEdit(QtCore.QDate.currentDate())
        self.before_date.setCalendarPopup(True)

        filters_layout.addWidget(self._label("包含"), 0, 0)
        filters_layout.addWidget(self.include_text, 0, 1)
        filters_layout.addWidget(self._label("排除"), 0, 2)
        filters_layout.addWidget(self.exclude_text, 0, 3)
        filters_layout.addWidget(self._label("名称通配"), 1, 0)
        filters_layout.addWidget(self.name_glob, 1, 1)
        filters_layout.addWidget(self._label("条目类型"), 1, 2)
        filters_layout.addLayout(type_row, 1, 3)
        filters_layout.addWidget(self._label("最小大小"), 2, 0)
        filters_layout.addLayout(self.min_size_row, 2, 1)
        filters_layout.addWidget(self._label("最大大小"), 2, 2)
        filters_layout.addLayout(self.max_size_row, 2, 3)
        filters_layout.addWidget(self.after_check, 3, 0)
        filters_layout.addWidget(self.after_date, 3, 1)
        filters_layout.addWidget(self.before_check, 3, 2)
        filters_layout.addWidget(self.before_date, 3, 3)
        filters.layout().addLayout(filters_layout)
        body.addWidget(filters)

        button_row = QtWidgets.QHBoxLayout()
        button_row.addStretch(1)
        self.backup_cancel = QtWidgets.QPushButton("取消任务")
        self.backup_cancel.setObjectName("DangerButton")
        self.backup_cancel.setEnabled(False)
        self.backup_cancel.clicked.connect(self._cancel_current)
        self.backup_start = QtWidgets.QPushButton("开始备份")
        self.backup_start.setObjectName("PrimaryButton")
        self.backup_start.clicked.connect(self._start_backup)
        button_row.addWidget(self.backup_cancel)
        button_row.addWidget(self.backup_start)
        body.addLayout(button_row)
        body.addStretch(1)

        self._sync_password_state()
        return page

    def _build_restore_page(self) -> QtWidgets.QWidget:
        """创建恢复页。"""

        page, body = self._page("恢复数据", "恢复时会自动完成解密、归档 SHA-256 和逐文件 SHA-256 校验。")

        panel = self._panel("恢复设置")
        layout = panel.layout()
        layout.setSpacing(12)
        self.restore_package = PathRow("备份包", "open", "选择 .pbackup 文件")
        self.restore_dest = PathRow("目标目录", "directory", "选择恢复到的文件夹")
        self.restore_password = QtWidgets.QLineEdit()
        self.restore_password.setEchoMode(QtWidgets.QLineEdit.Password)
        self.restore_password.setPlaceholderText("加密备份包需要输入密码")
        pwd_row = QtWidgets.QHBoxLayout()
        pwd_row.addWidget(self._label("密码"))
        pwd_row.addWidget(self.restore_password, 1)
        self.overwrite_check = QtWidgets.QCheckBox("覆盖同名文件")
        layout.addWidget(self.restore_package)
        layout.addWidget(self.restore_dest)
        layout.addLayout(pwd_row)
        layout.addWidget(self.overwrite_check)
        body.addWidget(panel)

        button_row = QtWidgets.QHBoxLayout()
        button_row.addStretch(1)
        self.restore_cancel = QtWidgets.QPushButton("取消任务")
        self.restore_cancel.setObjectName("DangerButton")
        self.restore_cancel.setEnabled(False)
        self.restore_cancel.clicked.connect(self._cancel_current)
        self.restore_start = QtWidgets.QPushButton("开始恢复")
        self.restore_start.setObjectName("PrimaryButton")
        self.restore_start.clicked.connect(self._start_restore)
        button_row.addWidget(self.restore_cancel)
        button_row.addWidget(self.restore_start)
        body.addLayout(button_row)
        body.addStretch(1)
        return page

    def _build_log_page(self) -> QtWidgets.QWidget:
        """创建日志页。"""

        page, body = self._page("运行日志", "查看备份和恢复过程中的校验事件、进度和错误。")
        self.log_view = QtWidgets.QTextEdit()
        self.log_view.setObjectName("LogView")
        self.log_view.setReadOnly(True)
        body.addWidget(self.log_view, 1)
        clear_button = QtWidgets.QPushButton("清空日志")
        clear_button.clicked.connect(self.log_view.clear)
        row = QtWidgets.QHBoxLayout()
        row.addStretch(1)
        row.addWidget(clear_button)
        body.addLayout(row)
        return page

    def _build_progress_footer(self) -> QtWidgets.QFrame:
        """创建底部进度区域。"""

        footer = QtWidgets.QFrame()
        footer.setObjectName("ProgressFooter")
        layout = QtWidgets.QGridLayout(footer)
        layout.setContentsMargins(14, 12, 14, 12)
        layout.setHorizontalSpacing(12)

        self.stage_label = QtWidgets.QLabel("就绪")
        self.stage_label.setObjectName("StageLabel")
        self.current_label = QtWidgets.QLabel("等待任务")
        self.current_label.setObjectName("CurrentLabel")
        self.current_label.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
        self.progress_bar = QtWidgets.QProgressBar()
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)

        layout.addWidget(self.stage_label, 0, 0)
        layout.addWidget(self.current_label, 0, 1)
        layout.addWidget(self.progress_bar, 1, 0, 1, 2)
        return footer

    def _page(self, title: str, subtitle: str) -> tuple[QtWidgets.QWidget, QtWidgets.QVBoxLayout]:
        """创建统一页头和可滚动正文。"""

        page = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(page)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(14)

        title_label = QtWidgets.QLabel(title)
        title_label.setObjectName("PageTitle")
        subtitle_label = QtWidgets.QLabel(subtitle)
        subtitle_label.setObjectName("PageSubtitle")
        layout.addWidget(title_label)
        layout.addWidget(subtitle_label)

        scroll = QtWidgets.QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QtWidgets.QFrame.NoFrame)
        inner = QtWidgets.QWidget()
        body = QtWidgets.QVBoxLayout(inner)
        body.setContentsMargins(0, 0, 4, 0)
        body.setSpacing(14)
        scroll.setWidget(inner)
        layout.addWidget(scroll, 1)
        return page, body

    def _panel(self, title: str) -> QtWidgets.QFrame:
        """创建半透明内容面板。"""

        panel = QtWidgets.QFrame()
        panel.setObjectName("Panel")
        panel_layout = QtWidgets.QVBoxLayout(panel)
        panel_layout.setContentsMargins(16, 14, 16, 16)
        heading = QtWidgets.QLabel(title)
        heading.setObjectName("PanelTitle")
        panel_layout.addWidget(heading)
        return panel

    def _label(self, text: str) -> QtWidgets.QLabel:
        """创建表单标签。"""

        label = QtWidgets.QLabel(text)
        label.setObjectName("FieldLabel")
        return label

    def _switch_page(self, index: int) -> None:
        """切换侧边栏页面并同步按钮选中态。"""

        self.stack.setCurrentIndex(index)
        for i, button in enumerate(self.nav_buttons):
            button.setChecked(i == index)

    def _choose_background(self) -> None:
        """选择图片作为背景并保存用户偏好。"""

        current = str(self.background_path.parent if self.background_path.is_file() else PROJECT_ROOT)
        selected, _ = QtWidgets.QFileDialog.getOpenFileName(
            self,
            "选择背景图片",
            current,
            "图片文件 (*.jpg *.jpeg *.png *.bmp *.webp);;所有文件 (*)",
        )
        if not selected:
            return
        selected_path = Path(selected)
        if not self.background_widget.set_background(selected_path):
            self._show_error("无法读取所选图片，请选择有效的 JPG、PNG、BMP 或 WebP 文件。")
            return
        self.background_path = selected_path
        self.settings.setValue("appearance/background_path", str(selected_path))
        self._log("ok", f"背景图片已更换：{selected_path}")

    def _reset_background(self) -> None:
        """恢复项目自带的默认背景图片。"""

        if not self.background_widget.set_background(BACKGROUND_PATH):
            self._show_error(f"默认背景图片无法读取：{BACKGROUND_PATH}")
            return
        self.background_path = BACKGROUND_PATH
        self.settings.remove("appearance/background_path")
        self._log("info", "已恢复默认背景图片。")

    def _start_backup(self) -> None:
        """收集备份页参数并启动后台备份。"""

        try:
            options = self._backup_options()
        except GuiError as exc:
            self._show_error(str(exc))
            return
        self._start_worker(options)

    def _start_restore(self) -> None:
        """收集恢复页参数并启动后台恢复。"""

        package = self.restore_package.text()
        dest = self.restore_dest.text()
        if not package or not dest:
            self._show_error("请选择备份包和目标目录。")
            return
        self._start_worker(
            {
                "arguments": [
                    "restore",
                    "--package", package,
                    "--destination", dest,
                    "--overwrite", "1" if self.overwrite_check.isChecked() else "0",
                ],
                "password": self.restore_password.text(),
            }
        )

    def _backup_options(self) -> dict[str, Any]:
        """把备份页控件值转换为 C++ 后端命令参数。"""

        source = self.backup_source.text()
        output = self.backup_output.text()
        if not source or not output:
            raise GuiError("请选择要备份的文件或文件夹，以及备份包保存位置。")
        output_path = Path(output)
        if not output_path.suffix:
            output_path = output_path.with_suffix(".pbackup")
            self.backup_output.set_text(str(output_path))

        encryption = str(self.encryption_combo.currentData())
        password = self.password_edit.text()
        confirm = self.password_confirm_edit.text()
        if encryption != "none":
            if not password:
                raise GuiError("请选择加密方式后必须填写密码。")
            if password != confirm:
                raise GuiError("两次输入的密码不一致。")

        type_filter: set[str] = set()
        if self.type_file.isChecked():
            type_filter.add("file")
        if self.type_dir.isChecked():
            type_filter.add("dir")
        if self.type_symlink.isChecked():
            type_filter.add("symlink")

        arguments = [
            "backup",
            "--source", source,
            "--output", str(output_path),
            "--compression", str(self.compression_combo.currentData()),
            "--encryption", encryption,
            "--kdf", str(int(self.kdf_spin.value())),
        ]
        for term in split_terms(self.include_text.toPlainText()):
            arguments.extend(["--include", term])
        for term in split_terms(self.exclude_text.toPlainText()):
            arguments.extend(["--exclude", term])
        if self.name_glob.text().strip():
            arguments.extend(["--glob", self.name_glob.text().strip()])
        for entry_type in sorted(type_filter):
            arguments.extend(["--type", entry_type])
        min_bytes = self._size_to_bytes(self.min_size_spin, self.min_size_unit)
        if min_bytes > 0:
            arguments.extend(["--min-size", str(min_bytes)])
        max_bytes = self._size_to_bytes(self.max_size_spin, self.max_size_unit)
        if max_bytes > 0:
            arguments.extend(["--max-size", str(max_bytes)])
        if self.after_check.isChecked():
            arguments.extend(["--mtime-after", str(int(self._qdate_timestamp(self.after_date.date(), end_of_day=False)))])
        if self.before_check.isChecked():
            arguments.extend(["--mtime-before", str(int(self._qdate_timestamp(self.before_date.date(), end_of_day=True)))])
        return {"arguments": arguments, "password": password}

    def _start_worker(self, payload: dict[str, Any]) -> None:
        """统一启动后台任务。"""

        if self.current_worker is not None:
            self._show_error("已有任务正在运行，请先等待或取消。")
            return
        self.progress_bar.setValue(0)              # 重置进度条
        self.stage_label.setText("运行中")         # 更新状态标签
        self.current_label.setText("任务启动中")   # 更新当前操作提示
        self._set_busy(True)                        # 锁定界面控件防止重复操作

        worker = OperationWorker(payload, self)
        worker.progress_changed.connect(self._on_progress)     # 连接进度更新信号
        worker.log_emitted.connect(self._log)                     # 连接日志信号
        worker.succeeded.connect(self._on_success)                 # 连接成功信号
        worker.failed.connect(self._on_failure)                    # 连接失败信号
        worker.finished.connect(self._on_worker_finished)          # 连接线程结束信号
        self.current_worker = worker
        worker.start()

    def _cancel_current(self) -> None:
        """请求取消当前后台任务。"""

        if self.current_worker is not None:
            self.current_worker.cancel()
            self._log("warn", "已请求取消任务，正在等待后台安全退出。")

    def _on_progress(self, event: dict[str, Any]) -> None:
        """接收后台进度并刷新底部状态。"""

        total_bytes = int(event.get("total_bytes", 0))
        done_bytes = int(event.get("done_bytes", 0))
        total_files = int(event.get("total_files", 0))
        done_files = int(event.get("done_files", 0))
        if total_bytes > 0:
            percent = min(100, int(done_bytes * 100 / total_bytes))
        elif total_files > 0:
            percent = min(100, int(done_files * 100 / total_files))
        else:
            percent = 0
        self.progress_bar.setValue(percent)
        self.stage_label.setText(str(event.get("stage", "处理中")))
        current = str(event.get("current_path") or event.get("message") or "处理中")
        self.current_label.setText(f"{done_files}/{total_files}  {current}")

    def _on_success(self, result: object) -> None:
        """后台任务成功完成。"""

        self.progress_bar.setValue(100)
        self.stage_label.setText("完成")
        self.current_label.setText(str(result))
        self._log("ok", f"任务完成：{result}")

    def _on_failure(self, message: str) -> None:
        """后台任务失败。"""

        if message == "任务已取消。":
            self.stage_label.setText("已取消")
            self.current_label.setText(message)
            self._log("warn", message)
            return
        self.stage_label.setText("失败")
        self.current_label.setText(message)
        self._log("error", message)
        self._show_error(message)

    def _on_worker_finished(self) -> None:
        """线程结束后的界面复位。"""

        self.current_worker = None
        self._set_busy(False)
        self._sync_password_state()

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:  # noqa: N802 - Qt 虚函数命名。
        """关闭窗口前通知 C++ 后端退出，避免遗留半成品。"""

        worker = self.current_worker  # 获取当前后台任务引用
        if worker is not None and worker.isRunning():  # 如果有任务正在运行
            worker.cancel()  # 先请求 C++ 后端安全取消
            if not worker.wait(5000):  # 等待最多 5 秒
                process = worker.process
                if process is not None and process.poll() is None:
                    process.terminate()
                worker.wait(2000)
        super().closeEvent(event)

    def _sync_password_state(self) -> None:
        """根据加密方式启用或禁用密码输入。"""

        enabled = str(self.encryption_combo.currentData()) != "none"
        self.password_edit.setEnabled(enabled)
        self.password_confirm_edit.setEnabled(enabled)
        self.kdf_spin.setEnabled(enabled)
        if not enabled:
            self.password_edit.clear()
            self.password_confirm_edit.clear()

    def _set_busy(self, busy: bool) -> None:
        """任务运行期间锁定会影响参数的一组控件。"""

        for widget in [
            self.backup_start,
            self.restore_start,
            self.compression_combo,
            self.encryption_combo,
            self.password_edit,
            self.password_confirm_edit,
            self.restore_password,
        ]:
            widget.setEnabled(not busy)
        self.backup_cancel.setEnabled(busy)
        self.restore_cancel.setEnabled(busy)

    def _log(self, level: str, message: str) -> None:
        """向日志页追加彩色日志。"""

        colors = {
            "info": "#2a6e8a",
            "warn": "#b8860b",
            "error": "#c0392b",
            "ok": "#1a7a42",
        }
        stamp = dt.datetime.now().strftime("%H:%M:%S")
        color = colors.get(level, "#d8e7ff")
        self.log_view.append(f'<span style="color:{color};">[{stamp}] [{level.upper()}] {escape_html(message)}</span>')

    def _show_error(self, message: str) -> None:
        """弹出错误提示。"""

        QtWidgets.QMessageBox.warning(self, "提示", message)

    def _set_combo_value(self, combo: QtWidgets.QComboBox, value: str) -> None:
        """按数据值选中下拉框条目。"""

        for index in range(combo.count()):
            if combo.itemData(index) == value:
                combo.setCurrentIndex(index)
                return

    def _qdate_timestamp(self, qdate: QtCore.QDate, end_of_day: bool) -> float:
        """把 QDate 转成本地时间戳。"""

        py_date = qdate.toPyDate()
        py_time = dt.time(23, 59, 59) if end_of_day else dt.time(0, 0, 0)
        return dt.datetime.combine(py_date, py_time).timestamp()

    def _size_to_bytes(self, spin: QtWidgets.QSpinBox, unit_combo: QtWidgets.QComboBox) -> int:
        """根据数值和单位下拉框计算字节数。"""

        value = spin.value()
        multiplier = int(unit_combo.currentData())
        return value * multiplier

    def _apply_style(self) -> None:
        """应用统一 QSS 样式 - 小清新风格，半透明模块充分展示背景图。"""

        self.setStyleSheet(
            """
            QWidget {
                font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
                font-size: 14px;
                color: #2d3a2e;
            }
            QFrame#Sidebar {
                background: rgba(255, 255, 255, 120);
                border: 1px solid rgba(130, 190, 160, 80);
                border-radius: 12px;
            }
            QFrame#ContentShell {
                background: rgba(255, 255, 255, 105);
                border: 1px solid rgba(130, 190, 160, 80);
                border-radius: 12px;
            }
            QLabel#AppTitle {
                font-size: 24px;
                font-weight: 700;
                color: #2e7d56;
            }
            QLabel#AppSubtitle {
                color: #6b9e85;
                font-size: 13px;
            }
            QLabel#PageSubtitle, QLabel#CurrentLabel {
                color: #5a7d6a;
            }
            QLabel#SidebarInfo {
                color: #4a7a62;
                background: rgba(200, 235, 210, 100);
                border-radius: 8px;
                padding: 12px;
                line-height: 1.4;
            }
            QPushButton {
                min-height: 34px;
                padding: 7px 14px;
                border-radius: 8px;
                border: 1px solid rgba(130, 200, 160, 120);
                background: rgba(255, 255, 255, 140);
                color: #3a6b50;
                font-weight: 500;
            }
            QPushButton:hover {
                background: rgba(200, 240, 215, 160);
                border-color: rgba(100, 185, 140, 160);
            }
            QPushButton:checked {
                background: rgba(76, 175, 120, 200);
                border-color: rgba(76, 175, 120, 220);
                color: white;
                font-weight: 600;
            }
            QPushButton#PrimaryButton {
                background: rgba(76, 175, 120, 210);
                border-color: rgba(76, 175, 120, 230);
                color: white;
                font-weight: 700;
                min-width: 118px;
            }
            QPushButton#PrimaryButton:hover {
                background: rgba(56, 155, 100, 230);
            }
            QPushButton#DangerButton {
                background: rgba(220, 110, 110, 180);
                border-color: rgba(220, 110, 110, 200);
                color: white;
            }
            QPushButton#DangerButton:hover {
                background: rgba(200, 90, 90, 210);
            }
            QPushButton:disabled {
                color: #a0b5a8;
                background: rgba(200, 210, 200, 80);
                border-color: rgba(180, 200, 185, 80);
            }
            QLabel#PageTitle {
                font-size: 26px;
                font-weight: 700;
                color: #2e7d56;
            }
            QFrame#Panel {
                background: rgba(255, 255, 255, 95);
                border: 1px solid rgba(160, 210, 180, 100);
                border-radius: 10px;
            }
            QFrame#ProgressFooter {
                background: rgba(255, 255, 255, 100);
                border: 1px solid rgba(160, 210, 180, 100);
                border-radius: 10px;
            }
            QLabel#PanelTitle {
                font-size: 17px;
                font-weight: 700;
                color: #2e7d56;
                margin-bottom: 4px;
            }
            QLabel#FieldLabel {
                color: #3d6b52;
                font-weight: 600;
            }
            QLabel#StageLabel {
                color: #2e7d56;
                font-weight: 600;
            }
            QLineEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit {
                min-height: 34px;
                border-radius: 8px;
                border: 1px solid rgba(160, 210, 180, 130);
                background: rgba(255, 255, 255, 160);
                color: #2d3a2e;
                selection-background-color: rgba(76, 175, 120, 160);
                selection-color: white;
                padding: 4px 8px;
            }
            QLineEdit:focus, QTextEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus {
                border-color: rgba(76, 175, 120, 200);
            }
            QTextEdit#LogView {
                background: rgba(255, 255, 255, 140);
                border: 1px solid rgba(160, 210, 180, 110);
                font-family: "Cascadia Mono", "Consolas", monospace;
                font-size: 13px;
                color: #2d3a2e;
            }
            QCheckBox {
                color: #3a6b50;
                spacing: 8px;
            }
            QCheckBox::indicator {
                width: 16px;
                height: 16px;
                border-radius: 4px;
                border: 1px solid rgba(130, 190, 160, 160);
                background: rgba(255, 255, 255, 180);
            }
            QCheckBox::indicator:checked {
                background: rgba(76, 175, 120, 210);
                border-color: rgba(76, 175, 120, 230);
            }
            QProgressBar {
                min-height: 14px;
                border-radius: 7px;
                background: rgba(200, 230, 210, 120);
                text-align: center;
                color: transparent;
            }
            QProgressBar::chunk {
                border-radius: 7px;
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 rgba(76, 175, 120, 220), stop:1 rgba(100, 200, 150, 220));
            }
            QScrollArea {
                background: transparent;
            }
            QScrollArea > QWidget > QWidget {
                background: transparent;
            }
            QComboBox::drop-down {
                border: none;
                width: 28px;
            }
            QComboBox QAbstractItemView {
                background: rgba(245, 255, 248, 240);
                border: 1px solid rgba(160, 210, 180, 150);
                border-radius: 6px;
                color: #2d3a2e;
                selection-background-color: rgba(76, 175, 120, 160);
                selection-color: white;
            }
            QScrollBar:vertical {
                background: transparent;
                width: 8px;
                border-radius: 4px;
            }
            QScrollBar::handle:vertical {
                background: rgba(130, 190, 160, 120);
                border-radius: 4px;
                min-height: 30px;
            }
            QScrollBar::handle:vertical:hover {
                background: rgba(100, 170, 140, 160);
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
            QScrollBar:horizontal {
                background: transparent;
                height: 8px;
                border-radius: 4px;
            }
            QScrollBar::handle:horizontal {
                background: rgba(130, 190, 160, 120);
                border-radius: 4px;
                min-width: 30px;
            }
            """
        )


def escape_html(text: str) -> str:
    """转义日志中的 HTML 特殊字符。"""

    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def main() -> int:
    """程序入口，设置高 DPI 和全局字体后显示主窗口。"""

    try:
        QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
        QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)
    except Exception:
        pass

    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("数据备份工具")
    app.setFont(QtGui.QFont("Microsoft YaHei UI", 10))
    window = MainWindow()
    window.show()
    return exec_app(app)
