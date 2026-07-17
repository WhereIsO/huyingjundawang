# 数据备份工具（Python GUI / C++ Core）

项目现在采用清晰的双层结构：前端 GUI 使用 Python + PyQt5；扫描、筛选、归档、压缩、加密、恢复和校验全部使用 C++ 实现。Python 不再包含备份业务核心。

## 运行

1. 安装 GUI 依赖：

   ```powershell
   pip install -r requirements.txt
   ```

2. 启动：

   ```powershell
   python main.py
   ```

   Windows 也可以双击 `启动.bat`。仓库已包含编译好的 `backend/pbackup_core.exe`。

## 重新编译 C++ 核心

支持 Windows 7 及更高版本，以及支持 C++17 的较新 MinGW-w64（推荐 GCC 10+）：

```powershell
cd backend
.\build.bat
```

也可以先通过 `CXX` 环境变量指定编译器。C++ 核心只链接 Windows 自带的 CNG 加密 API，不依赖 Python 加密库或第三方压缩 DLL。Windows 8 及以上会自动启用 MSZIP、XPRESS、XPRESS Huffman 与 LZMS；Windows 7 会自动降级为 Stored 无压缩模式，仍可创建、恢复和校验未压缩备份包。

## 功能

- 备份源既可以是一个完整文件夹，也可以是单独的文件。
- 输出扩展名为 `.pbackup`，格式标识为 `PBACKUP-CPP3`。
- 压缩：Stored、MSZIP/Deflate、XPRESS、XPRESS Huffman、LZMS。
- 加密：AES-256-GCM、ChaCha20-Poly1305，密钥均通过 PBKDF2-HMAC-SHA256 派生。
- 每个文件保存 SHA-256，外层归档也保存 SHA-256；加密负载按块进行认证。
- GUI 不再提供独立校验页面；恢复数据时会自动验证认证标签、归档 SHA-256、归档结构和逐文件 SHA-256，校验失败时不会继续恢复。
- 包内只保存 UTF-8 相对路径，恢复时拒绝绝对路径、`..`、驱动器路径以及经过符号链接父目录的写入。
- C++ 后端通过逐行 JSON 向 GUI 报告进度、日志、结果和错误。
- 密码通过子进程环境传递，不出现在命令行参数中。
- GUI 可以选择 JPG、PNG、BMP 或 WebP 图片作为背景，并记住上次选择；也可以一键恢复默认背景。

## 代码结构

- `main.py`：Python GUI 入口。
- `app/gui.py`：PyQt 窗口、输入校验、C++ 进程通信、进度与日志展示。
- `app/qt_compat.py`：PyQt5/PySide 兼容层。
- `backend/pbackup_core.cpp`：全部备份业务核心。
- `backend/build.bat`：C++17 构建脚本。
- `backend/pbackup_core.exe`：已编译后端。
- `background.jpg`：界面背景。

## 格式兼容性

C++ v3 使用新的流式归档格式，不兼容此前由 `app/backup_core.py` 生成的 `PBACKUP-PY2` 文件。这样可以彻底移除 Python 业务核心，并让大文件压缩和加密保持流式处理，避免整包载入内存。
