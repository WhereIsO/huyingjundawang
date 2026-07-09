# 数据备份工具 — 前端（GUI）说明文档

本目录是「数据备份软件」的图形界面部分，基于 **Qt 5 + CMake** 实现。界面只负责交互与展示，真正的备份/还原/压缩/加密逻辑由 `src/core` 纯 C++ 后端承担；两者通过抽象接口 `BackendAdapter` 解耦。当前同时保留 `MockBackend` 和 `RealBackend`：默认使用 Mock 演示界面，设置 `BACKUP_BACKEND_MODE=real` 后使用真实后端。

---

## 一、目录结构

```
gui/BackupTool/
├── CMakeLists.txt          # 构建脚本（含 MSVC /utf-8 修复）
└── src/
    ├── main.cpp            # 程序入口：装配 QApplication、全局字体、显示主窗口
    ├── MainWindow.{h,cpp}  # 主窗口：三标签页 + 进度条 + 日志面板，连接后端信号
    ├── BackupTab.{h,cpp}   # 备份页：选源目录 / 选输出包 / 压缩·加密 / 开始·取消
    ├── RestoreTab.{h,cpp}  # 还原页：选备份包 / 选目标目录 / 密码 / 覆盖
    ├── FilterTab.{h,cpp}   # 筛选页：路径/类型/名称/时间/大小/用户 六类过滤
    ├── LogPanel.{h,cpp}    # 实时彩色日志面板（QPlainTextEdit）
    ├── PathPicker.{h,cpp}  # 「标签+输入框+浏览」组合控件，可复用
    ├── Theme.{h,cpp}       # 统一大字号 / 大按钮 / 高对比度主题
    ├── BackendAdapter.{h,cpp}  # 抽象后端接口 + Mock 实现 + 工厂函数
    └── MockBackend.{h,cpp}     # 占位单元（Mock 实现体在 BackendAdapter.cpp 内）
```

---

## 二、软件架构

### 2.1 前后端解耦

界面不直接依赖 `backup_core`，而是面向抽象基类 `BackendAdapter` 编程：

```cpp
class BackendAdapter : public QObject {
public:
    virtual bool startBackup(const BackupRequest&, const FilterSpec&) = 0;
    virtual bool startRestore(const RestoreRequest&) = 0;
    virtual void cancel() = 0;
    virtual bool isRunning() const = 0;
signals:
    void progress(const Progress& p);              // 进度回调
    void log(int level, const QString& text);      // 0=Info 1=Warn 2=Error 3=Ok
    void finished(bool success, const QString& summary);
    void failed(const QString& reason);
};
```

- 所有耗时操作都在工作线程执行，通过 Qt 信号异步回报进度与日志，界面永不卡死。
- 跨线程传递的 `Progress` 结构体已用 `Q_DECLARE_METATYPE` + `qRegisterMetaType` 注册。

### 2.2 数据契约（GUI ↔ 后端）

| 结构体 | 作用 | 关键字段 |
|--------|------|----------|
| `BackupRequest`  | 备份参数 | `sourceDir` / `outputPkg` / `compress` / `encrypt` / `password` |
| `RestoreRequest` | 还原参数 | `pkg` / `destDir` / `password` / `overwrite` |
| `FilterSpec`     | 六类筛选 | 路径含/排、名称通配、类型、大小区间、时间区间、属主 SID |
| `Progress`       | 进度快照 | 总/已字节、总/已文件数、当前文件、阶段、告警；`percent()` 计算百分比 |

后端 `RealBackend` 只要实现 `BackendAdapter` 这四个纯虚函数、按约定发出四个信号即可无缝接入，界面代码一行都不用改。

### 2.3 工厂切换

```cpp
std::unique_ptr<BackendAdapter> createBackend(QObject* parent);
```

读取环境变量 `BACKUP_BACKEND_MODE`：

- 未设置或非 `real` → 返回 `MockBackend`（默认，保证任何环境都能演示）；
- 等于 `real` → 返回 `RealBackend`，调用真实 C++ 后端执行备份/还原。

---

## 三、构建

### 3.1 环境要求

- CMake ≥ 3.16
- Qt 5.12+（本机验证于 Qt 5.15.2）
- 编译器需与 Qt 的构建 ABI 一致（**重要，见 3.3**）

### 3.2 标准构建命令

```bat
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt根目录>
cmake --build build
```

生成物：`build/BackupTool.exe`。

### 3.3 两个已踩过的坑（务必注意）

1. **编译器 ABI 必须与 Qt 匹配。**
   若 Qt 是用 MSVC 编译的（例如随 Anaconda 分发的 `Qt5Widgets_conda.lib`、mkspec 为 `win32-msvc`），就必须用 **MSVC (`cl.exe`)** 构建，不能用 MinGW g++——否则编译能过，链接会报成片的 `undefined reference to __imp_...`。反之亦然。
   查看 Qt 的 mkspec：`qmake -query QMAKE_XSPEC`。

2. **MSVC 必须加 `/utf-8`。**
   本项目源码为 UTF-8 且含中文注释/字符串。MSVC 默认按系统代码页（GBK）读源文件，会把多字节序列误读成引号或反斜杠，报大量 `C2001: newline in constant`、`C1057: unexpected end of file`。CMakeLists 已内置修复：
   ```cmake
   if (MSVC)
       add_compile_options(/utf-8)
   endif()
   ```

3. **避免超长 / 含中文的构建路径。**
   MinGW 工具链在含中文的路径下会报 `Illegal byte sequence`；MSVC 对象文件路径过长（>250 字符）也会告警。`build_msvc.bat` 默认会把源码镜像到 `%TEMP%\BackupTool_src`，并构建到 `%TEMP%\BackupTool_build`。

### 3.4 MSVC 一键构建（Windows + VS 2022）

仓库随附 `build_msvc.bat`。由于课程目录包含中文，Qt AUTOMOC 可能生成损坏的 include 路径；脚本会先把当前工程镜像到 ASCII 临时路径，再 `call vcvars64.bat` 进入 MSVC 环境，用 Ninja 构建并运行测试：

```bat
build_msvc.bat
```

默认生成物：

- `%TEMP%\BackupTool_build\BackupTool.exe`
- `%TEMP%\BackupTool_build\backup_tests.exe`（本机存在 GTest 或允许下载时）
- `%TEMP%\BackupTool_build\real_backend_smoke.exe`（本机存在 GTest 或允许下载时）

当前后端测试目标：

- `backup_tests`：纯 C++ core 单元测试，不依赖 Qt。
- `real_backend_smoke`：无窗口 Qt smoke test，验证 `BACKUP_BACKEND_MODE=real` 时工厂返回 `RealBackend`，并完成真实备份→还原闭环。

可选环境变量：

```bat
set QT_PREFIX=C:\Qt\5.15.2\msvc2019_64
set BACKUP_ALLOW_GTEST_DOWNLOAD=ON
build_msvc.bat
```

CMake 默认不联网下载 GoogleTest；若本机没有 GTest，会跳过测试目标。需要自动下载时显式设置 `BACKUP_ALLOW_GTEST_DOWNLOAD=ON`。

---

## 四、运行与使用

1. 交付目录中双击 `启动软件-本地.bat` 可进入真实后端；双击 `启动软件-仅演示界面.bat` 可进入 Mock 演示后端。`bin` 目录已包含 Qt/VC 运行库，不依赖 Anaconda 或固定盘符。
2. **① 备份**：选源目录 → 选备份包保存路径（`*.pbackup`）→ 按需勾选哈夫曼压缩 / AES-256 加密（勾选加密后填写密码）→ 点「开始备份」。
3. **② 筛选**：设置路径、类型、名称、大小、时间、属主六类条件，真实后端会在备份时应用这些条件；Mock 后端仅用于界面演示。
4. **③ 还原**：选备份包 → 选目标目录 → 如为加密包填密码 → 可选「覆盖同名文件」→ 点「开始还原」。
5. 底部进度条实时显示百分比与阶段，日志面板按 Info/Warn/Error/Success 分色滚动输出。

### 4.1 切换真实后端

默认未设置 `BACKUP_BACKEND_MODE` 时使用 Mock 后端，便于界面演示。设置为 `real` 后使用 `RealBackend`：

```powershell
$env:BACKUP_BACKEND_MODE = "real"
& ".\bin\BackupTool.exe"
```

### 4.2 无障碍 / 大字号

界面默认使用常规桌面字号、清晰按钮和高对比度配色。可通过环境变量覆盖：

```bat
set BACKUP_FONT_SIZE=18
```

（有效范围 8–24。）

---

## 五、真实后端接入状态

真实后端已经接入：

1. `src/RealBackend.{h,cpp}` 继承 `BackendAdapter`，实现 `startBackup / startRestore / cancel / isRunning`。
2. `src/core/` 为纯 C++ 后端核心，不依赖 Qt，包含扫描、筛选、元数据、归档、哈夫曼、AES-GCM、备份/还原任务。
3. `createBackend()` 已读取 `BACKUP_BACKEND_MODE=real` 并返回 `RealBackend`；否则仍返回 `MockBackend`。
4. `tests/core_tests.cpp` 覆盖 core 单元测试。
5. `tests/real_backend_smoke.cpp` 覆盖 RealBackend 工厂切换和真实备份→还原闭环。
6. `.pbackup` 字节格式说明见 `docs/format.md` 和 `src/core/docs/format.md`。

数据结构定义见 `BackendAdapter.h`，即 GUI 与后端的唯一契约文件。
