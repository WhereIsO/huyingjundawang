# 交给 Codex 的任务书（数据备份软件 · 后端）

> 使用方式：把本文件**整篇**粘贴给 Codex 即可。它已经交代清楚运行环境、目录、契约与任务，不依赖任何口头补充。
> 配套文件：同目录 `00_backend_goal.md` 是更详细的后端设计（备份包格式、模块划分、测试清单）。本文件是"总入口 + 前端对接契约"，两者冲突时**以本文件的接口契约为准**。

---

你是本项目的**后端开发**。这是一门 Windows 平台的《软件开发综合实验》课程小组作业，做一款**数据备份软件**。前端 Qt GUI 已由另一名成员完成并通过编译，现在需要你实现后端核心逻辑，并让它**无缝接入已有前端**。

## 一、运行与编译环境（务必遵守，否则无法通过编译）

- 操作系统：Windows 11，x64。
- 编译器：**MSVC 14.44**（Visual Studio 2022 Community 的 `cl.exe`），C++17。
  **不要用 MinGW / g++**——本机的 Qt 是 MSVC 构建的，ABI 与 MinGW 不兼容，混用必然链接失败（本项目已踩过这个坑）。
- GUI 框架：**Qt 5.15.2（MSVC 版）**，位于 `G:\Anaconda3\Library`。
- 构建系统：CMake + Ninja。配置示例：
  ```
  cmake -S <src> -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_PREFIX_PATH=G:\Anaconda3\Library -DCMAKE_CXX_COMPILER=cl
  cmake --build <build>
  ```
  编译前需先 `call` VS 的 `vcvars64.bat` 进入 MSVC 环境（路径：`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat`）。
- **源码一律 UTF-8 编码**。CMake 里已对 MSVC 加了 `/utf-8`；你新增的所有文件也必须保存为 UTF-8，否则中文注释/字符串会触发 `C2001 / C1057`（本项目已踩过这个坑）。
- 路径陷阱：项目真实路径含中文（见下）。MinGW 及部分工具在中文路径下会报 `Illegal byte sequence`。若某工具在中文路径下异常，可把源码整体复制到纯 ASCII 短路径（如 `G:\_src`）再构建——这是已验证可行的规避手段，不是代码问题。

## 二、项目目录（真实路径，含中文，请在此工作）

项目根：
```
F:\Files\Payki in UESTC\学习与课程\VII. Senior I（2026-2027）\【工】软件开发综合实验-李忻洋\Project_BackupTool\
```

GUI 工程（**已完成，除指定处外禁止改动**）：
```
gui\BackupTool\
  CMakeLists.txt                ← 构建配置（你需要往里追加 core 源文件与单元测试）
  src\
    BackendAdapter.h / .cpp     ← 【前后端契约】以此为准
    MockBackend.h / .cpp        ← 演示用假后端；你完成后由 RealBackend 取代
    MainWindow / *Tab / LogPanel / PathPicker / Theme   ← 纯 UI，禁止改
```

你的后端代码放到：
```
gui\BackupTool\src\core\        ← 纯 C++ 核心逻辑（不依赖 Qt，便于单元测试）
gui\BackupTool\src\RealBackend.h / .cpp   ← 桥接层：把 core 包装成前端契约
```
（若你也想同时产出独立的 `backup_core` 静态库 + `backup_cli` 命令行 + `backup_tests` 测试三件套，按 `00_backend_goal.md` 的目录布局做，但**必须额外**提供下面的 `RealBackend` 桥接层，让 GUI 能直接用。）

## 三、前后端契约（权威版本，逐字对齐，禁止改签名）

前端只认识下面这个抽象类和这几个结构体（摘自真实的 `gui\BackupTool\src\BackendAdapter.h`）。你的桥接层必须原样匹配，命名空间是 **`pbackup::ui`**：

```cpp
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <memory>

namespace pbackup::ui {

struct BackupRequest {
    QString sourceDir;
    QString outputPkg;
    bool    compress   = true;     // 哈夫曼
    bool    encrypt    = false;    // AES-256-GCM
    QString password;              // 明文密码
};

struct RestoreRequest {
    QString pkg;
    QString destDir;
    QString password;
    bool    overwrite = false;
};

struct FilterSpec {                // 6 类筛选，每项为空 = 不过滤
    QStringList includePath;       // 路径包含子串
    QStringList excludePath;
    QString     nameGlob;          // 例: *.docx
    QString     typeFilter;        // 逗号分隔：file,dir,symlink,hardlink
    QString     sizeMin;           // 字节
    QString     sizeMax;
    QString     mtimeAfter;        // yyyy-MM-dd
    QString     mtimeBefore;
    QString     ownerSid;          // 留空 = 任何
};

struct Progress {
    quint64 totalBytes  = 0;
    quint64 doneBytes   = 0;
    quint32 totalFiles  = 0;
    quint32 doneFiles   = 0;
    QString currentFile;
    QString stage;                 // Scanning / Filtering / Writing / Verifying / Done
    QStringList warnings;
    int percent() const;           // doneBytes/totalBytes*100
};

class BackendAdapter : public QObject {
    Q_OBJECT
public:
    explicit BackendAdapter(QObject* parent = nullptr);
    virtual ~BackendAdapter() = default;

    // 返回 true 表示成功开始；后续通过 progress/failed/finished 信号异步回调。
    virtual bool startBackup(const BackupRequest& req, const FilterSpec& filter) = 0;
    virtual bool startRestore(const RestoreRequest& req) = 0;
    virtual void cancel() = 0;
    virtual bool isRunning() const = 0;

signals:
    void progress(const Progress& p);
    void log(int level, const QString& text);   // 0=Info 1=Warn 2=Error 3=Ok
    void finished(bool success, const QString& summary);
    void failed(const QString& reason);
};

// 工厂：当前返回 Mock；你要让它在 BACKUP_BACKEND_MODE=real 时返回 RealBackend。
std::unique_ptr<BackendAdapter> createBackend(QObject* parent = nullptr);

} // namespace pbackup::ui

Q_DECLARE_METATYPE(pbackup::ui::Progress)   // 已声明，Progress 可跨线程随信号传递
```

> ⚠️ 注意契约漂移：`00_backend_goal.md` §4 里 core 库用的是 `std::string / std::filesystem / std::function` 回调（`BackupOptions`、`BackupProgress` 等）；而前端 GUI 用的是 **Qt 类型 + Qt 信号**（`BackupRequest`、`Progress` 等，`pbackup::ui` 命名空间）。**这两套是不同层，不要混为一谈。** 你的 `RealBackend` 的职责就是在两者之间做翻译。

## 四、你的具体任务

**1. 纯 C++ 核心（`src\core\`，不 include 任何 Qt 头，用 `std::string` / `std::filesystem`）**
   - 基础功能（必做）：目录备份到单一 `.pbackup` 包；从 `.pbackup` 还原到指定目录。
   - `.pbackup` 是自定义二进制归档格式，格式规约见 `00_backend_goal.md` §2，请严格实现并在 `docs/format.md` 落地说明（magic、头部、文件表、数据区、CRC32/SHA-256 校验）。
   - 扩展功能，按加分项实现（尽量多做）：
     - **文件类型支持**：Windows 下符号链接 / 硬链接 / junction(重解析点)。
     - **元数据支持**：大小、属性(只读/隐藏/系统/存档)、时间戳、所有者 SID、ACL 摘要。
     - **6 类自定义筛选**：路径 / 类型 / 名称 / 时间 / 大小 / 用户（对应上面 `FilterSpec`，逻辑 AND）。
     - **哈夫曼压缩/解压**：**必须自研，禁止第三方压缩库**（用了扣一半分）。
     - **加密/解密**：AES-256-GCM。**仅此一项允许第三方**——用 Windows CNG(bcrypt) 或 OpenSSL，PBKDF2-SHA256 派生密钥；**禁止自己手写密码学算法**；错误密码必须失败，不能静默放行。
   - core 必须**可取消**（传入原子标志或回调返回 false 即中断）、**可回调进度**（提供 `std::function` 进度/日志回调，**不要**在 core 里依赖 Qt）。

**2. 桥接层 `RealBackend`（`src\RealBackend.h / .cpp`）**
   - 继承 `pbackup::ui::BackendAdapter`，实现四个纯虚函数 `startBackup / startRestore / cancel / isRunning`。
   - 在工作线程运行 core（参考现有 `MockBackend` 用 `QtConcurrent::run` 的写法），把 core 的进度/日志回调翻译成 `progress()` / `log()` / `finished()` / `failed()` 信号。
   - 负责 **Qt 类型 ↔ std 类型互转**：`QString` ↔ `std::string`（用 `toStdWString`/`fromStdWString` 走宽字符，避免中文乱码）、`FilterSpec` ↔ core 的筛选参数、core 的 `BackupProgress` ↔ Qt 的 `Progress`。
   - 日志级别映射：core 的 info/warn/error → `log(0/1/2, ...)`，成功用 `log(3, ...)`。

**3. 打开开关：修改 `BackendAdapter.cpp` 里的 `createBackend()`**
   - 当环境变量 `BACKUP_BACKEND_MODE=real` 时返回 `RealBackend`，否则仍回退 `MockBackend`。
   - **这是唯一允许你改的前端文件，且只改这一个函数体。**

**4. 更新 `gui\BackupTool\CMakeLists.txt`**
   - 把 `src\core\*`、`src\RealBackend.*` 加进 `add_executable` 的源文件列表。
   - 若引入 OpenSSL，加 `find_package(OpenSSL REQUIRED)` 并链接；若用 Windows CNG，链接 `bcrypt`（无需第三方，优先推荐，省去 OpenSSL 部署）。

**5. 单元测试（GoogleTest）**
   - 为 core 写单测，目标 **≥ 30 用例**，覆盖：备份/还原往返字节级一致、哈夫曼压缩解压可逆、加密解密可逆、每类筛选命中与排除、各类特殊文件与元数据保真、边界与异常（源不存在、包损坏/截断、密码错误、空目录、空文件、大文件、中文路径、取消中断）。
   - 测试 target 独立于 GUI，**不依赖 Qt**，`ctest` 一键跑。

## 五、硬性约束

- 除**加密**可用第三方（OpenSSL / Windows CNG）外，其余功能（尤其**压缩**）一律自研，不得引第三方库。
- 后台逻辑**禁止 Python 等脚本语言**。
- 不得修改除 `createBackend()` 之外的任何前端 UI 文件；契约结构体 / 签名**一个字都不能改**。
- core 层不依赖 Qt，保证脱离 GUI 单独编译测试。
- 所有新文件 UTF-8 编码；注释与日志用简体中文。

## 六、完成标准（Definition of Done）

- MSVC + Qt 5.15.2 下 `cmake --build` **全量通过**，生成 `BackupTool.exe`。
- 设 `BACKUP_BACKEND_MODE=real` 启动，能对真实目录完成**备份 → 还原**，进度条与日志面板正常刷新，还原结果与源目录字节级一致。
- GoogleTest 全绿（≥ 30 用例）。
- `src\core\` 下附一份 `.pbackup` 格式说明（`docs/format.md`）与 core 模块 README。

---

**请先输出：(1) 你的实现方案与模块划分；(2) `.pbackup` 格式的最终设计；(3) `RealBackend` 的类型转换与线程模型说明。确认无误后再开始写代码，并按 commit 粒度推进。**
