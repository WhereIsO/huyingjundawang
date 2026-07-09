# 数据备份软件 — 后端开发 Goal 文档（交给 GPT 实现）

> 用途：把课程要求 + 小组决策固化为可由 GPT 一次性产出的 C++ 后端代码与构建工程目标。
> 范围：**仅限后端（核心库 + CLI + GoogleTest 单元测试）**。Qt GUI、StarUML 图、实验报告由前端组负责。
> 项目根目录：`F:\Files\Payki in UESTC\学习与课程\VII. Senior I（2026-2027）\【工】软件开发综合实验-李忻洋\Project_BackupTool\`
> 输出语言：简体中文（注释、日志、错误信息均为中文）。
> 实现语言：C++（C++17），CMake ≥ 3.16。平台：Windows 10/11。

---

## 0. 业务上下文（必读）

电子科技大学《软件开发综合实验》第七学期大作业。要求小组（≤3 人）用面向对象方法学完成一款 Windows 下运行的**数据备份软件**，配套交付需求文档 / 系统设计文档 / 测试报告 / UML 图 / 答辩 PPT / 演示视频。

评分由四块组成：

| 维度 | 分数 |
|---|---|
| 基础分（数据备份 + 数据还原） | 40 |
| 扩展分（见 §3） | 上限 70，封顶项目总分 110 |
| 项目完成分（需求 10 + 设计 20 + 测试 20 + AI 证书 10 + 源码质量 10 + 答辩 10 + 演示 20） | 100 |
| 个人得分 = 项目完成分 × 项目难度分 / 100 | 组长封顶 100，组员 −5 |

注：扩展分中"加密解密"是**唯一**允许使用第三方库的扩展；其余扩展若用第三方库直接实现，对应分按 50% 计。后台逻辑禁止 Python 等脚本语言。

---

## 1. 工程目标（Deliverables）

GPT 在本目录交付：

```
Project_BackupTool/
├── CMakeLists.txt                # 顶层构建，配置 backup_core / backup_cli / backup_tests 三个 target
├── README.md                     # 简明构建运行说明（中文）
├── .gitignore
├── cmake/                        # FindXXX.cmake（如 FindOpenSSL）
├── include/pbackup/              # 公共头文件（库导出的 include 目录）
│   ├── types.hpp                 # FileEntry / Metadata / BackupOptions / BackupResult 等 POD
│   ├── scanner.hpp
│   ├── filter.hpp
│   ├── metadata.hpp
│   ├── archive.hpp               # ArchiveReader / ArchiveWriter 接口
│   ├── compression.hpp
│   ├── crypto.hpp
│   ├── backup_task.hpp
│   ├── restore_task.hpp
│   ├── logger.hpp
│   └── error.hpp                 # 统一的 BackupError 异常类 + ErrorCode 枚举
├── src/
│   ├── core/                     # BackupTask / RestoreTask 调度
│   ├── scanner/                  # 目录树扫描，识别文件类型与硬链接
│   ├── filter/                   # 6 类筛选规则
│   ├── metadata/                 # 元数据采集与恢复
│   ├── archive/                  # 备份包读写
│   ├── compression/              # 自实现哈夫曼压缩/解压
│   ├── crypto/                   # OpenSSL AES-256-GCM 加密/解密
│   ├── cli/                      # 命令行入口（main.cpp）
│   ├── logger/                   # 日志实现
│   └── utils/                    # 路径处理、文件 IO、字节缓冲、CRC32
├── tests/                        # GoogleTest 单元测试，至少 30 个用例
│   ├── test_scanner.cpp
│   ├── test_filter.cpp
│   ├── test_metadata.cpp
│   ├── test_archive.cpp
│   ├── test_compression.cpp
│   ├── test_crypto.cpp
│   ├── test_backup_task.cpp
│   ├── test_restore_task.cpp
│   └── test_cli.cpp
├── samples/                      # 用于演示与测试的样例目录树生成脚本（PowerShell）
└── docs/
    └── format.md                 # .pbackup 备份包字节格式规约（GUI 也要照此对接）
```

三个 CMake target：

- `backup_core`：静态库，给 GUI 与 CLI 复用。
- `backup_cli`：命令行可执行程序，提供 `backup` / `restore` / `verify` 三个子命令，**这是 GUI 之外的官方入口**。
- `backup_tests`：GoogleTest 可执行程序，`ctest` 一键全跑。

---

## 2. 备份包格式 `.pbackup`（必须严格执行，GUI/CLI/测试共享同一格式）

采用**单文件归档 + 集中索引**结构，字节序为小端。

```
┌────────────────────────────────────────────┐
│ FileHeader            固定 64 字节          │
├────────────────────────────────────────────┤
│ GlobalConfig          变长（TLV，紧随 Header）│
├────────────────────────────────────────────┤
│ IndexEntry[0..N-1]    N 个变长 TLV 顺序紧排 │
│   (相对路径、类型、大小、元数据、数据偏移)   │
├────────────────────────────────────────────┤
│ Payload (压缩后/加密后)                     │
│   [可选] Chunk[0..M-1]，按 FileEntry 顺序   │
├────────────────────────────────────────────┤
│ Trailer (整体 SHA-256 + 索引 CRC32)         │
└────────────────────────────────────────────┘
```

### 2.1 FileHeader（64 字节）

| 偏移 | 长度 | 字段 | 说明 |
|---|---|---|---|
| 0 | 8 | Magic | ASCII `"PBACKUP\0"` |
| 8 | 2 | VersionMajor | 当前 `1` |
| 10 | 2 | VersionMinor | 当前 `0` |
| 12 | 1 | Platform | `0x01` = Windows |
| 13 | 1 | Flags | bit0=压缩 bit1=加密 |
| 14 | 4 | IndexCount | 文件项数 |
| 18 | 4 | IndexOffset | IndexEntry 区起始偏移 |
| 22 | 4 | PayloadOffset | 数据区起始偏移 |
| 26 | 8 | CreatedUnixNs | 创建时间（纳秒） |
| 34 | 16 | Reserved | 0 |
| 50 | 8 | Salt | 加密时随机盐；明文备份写 0 |
| 58 | 2 | KdfIters | PBKDF2 迭代次数，默认 200000 |
| 60 | 4 | HeaderCRC32 | 前 60 字节的 CRC32 |

### 2.2 GlobalConfig（TLV 列表，写完后写入 `IndexOffset` 与 `PayloadOffset`）

至少包含：

- Tag `0x0001` ToolName（UTF-8）：`"BackupTool/1.0"`
- Tag `0x0002` CompressionAlgo：`0=HUFFMAN`
- Tag `0x0003` EncryptionAlgo：`0=NONE 1=AES-256-GCM`
- Tag `0x0004` SourceRootLen + SourceRoot（UTF-8，相对路径前缀基准）

### 2.3 IndexEntry

每个条目是一个 TLV：

```
Tag        2B   固定 0x10
Length     4B
RelPath    变长 UTF-8 以 \0 结尾
EntryType  1B   0=File 1=Dir 2=EmptyDir 3=Symlink 4=Hardlink 5=Junction 6=ReparsePoint
FileSize   8B   原始大小
MTimeNs    8B
ATimeNs    8B
CTimeNs    8B
Attr       4B   GetFileAttributes 结果
OwnerSid   变长 SID 字符串以 \0 结尾
AclDigest  32B  SHA-256 摘要（可选，全 0 表示未记录）
TargetPath 变长 仅链接类条目有，UTF-8\0
DataOffset 8B   Payload 内偏移；目录/链接类写 0
DataLen    8B   压缩+加密后的字节数；目录/链接类写 0
EntryCRC32 4B   该条目序列化字节的 CRC32
```

### 2.4 Payload

- 未压缩/未加密：直接拼接每个 FileEntry 的原始字节。
- 仅压缩：每条 FileEntry 数据先按哈夫曼压缩，**每条独立压缩**（支持随机访问解压）。
- 压缩+加密：先逐条压缩，整体 AES-256-GCM 加密（用 KDF 从密码 + Salt 派生 32 字节密钥），IV 紧随密文头部。
- 仅加密：跳压缩，整体加密。

### 2.5 Trailer

```
PayloadCRC32   4B  整个 Payload（含加密后的密文）的 CRC32
PackageSHA256  32B 整个包除本字段之外所有字节的 SHA-256
```

---

## 3. 功能清单与口径

### 3.1 基本要求（基础分 40，**必须**完成）

- `cli backup --src <dir> --out <file.pbackup> [--compress] [--encrypt --pass <pwd>]`
- `cli restore --pkg <file.pbackup> --dst <dir> [--pass <pwd>]`
- `cli verify --pkg <file.pbackup> [--pass <pwd>]` 仅校验完整性，不写盘。

### 3.2 扩展要求（本项目必做 7 项，覆盖扩展分 55+，其余 3 项明确不做）

| 扩展 | 满分 | 必做？ | 实现口径 |
|---|---|---|---|
| 文件类型支持 | 10 | **必做** | 至少识别/恢复：普通文件、普通目录、空目录、符号链接、目录联接点（junction）、硬链接。命名管道与设备在报告中说明不支持。 |
| 元数据支持 | 10 | **必做** | 保存并恢复：大小、属性、创建/修改/访问时间、Owner SID、ACL 摘要、链接目标/索引节点信息。ACL 完整还原尽力而为，失败记录警告不中断。 |
| 自定义备份筛选 | 各 3 共 18 | **必做 6 类全做** | 路径（包含/排除/前缀）、类型、名字（glob）、时间（m/a/c）、尺寸（min/max）、用户（owner）。逻辑 AND。 |
| 打包解包 | 10 | **必做** | 即 §2 备份包格式本身，单文件归档。 |
| 压缩解压 | 10 | **必做** | 自实现哈夫曼（不引第三方库，避免分折半）。每条 FileEntry 独立压缩。 |
| 加密解密 | 10 | **必做，使用 OpenSSL** | AES-256-GCM；PBKDF2-SHA256 派生密钥（200000 次迭代，16B 随机 salt）；错误密码必须失败，不能静默放行。 |
| 图形界面 | 10 | **由前端组实现** | Qt 5（与课程示例对齐）。本 goal 文档**不**交付 GUI 代码，前端组在此基础上开发。 |
| 定时备份 | 10 | 不做 | 报告里说明范围与原因。 |
| 实时备份 | 15 | 不做 | 同上。 |
| 网络备份 | 10+ | 不做 | 同上。 |

---

## 4. 模块 API 约定（GUI 会直接调用，**这是契约**）

所有公共类型在 `include/pbackup/` 下，命名空间 `pbackup`。`backup_core` 静态库导出以下 C++ 类（C++17，`std::filesystem`、`std::expected` 没有的话用 `tl::expected` 或自定义 `Result<T>`）。

```cpp
// types.hpp
namespace pbackup {
enum class EntryType : uint8_t { File=0, Dir=1, EmptyDir=2,
                                 Symlink=3, Hardlink=4,
                                 Junction=5, ReparsePoint=6 };

struct FileTimes { int64_t mtime_ns, atime_ns, ctime_ns; };
struct Metadata {
    uint64_t size;
    uint32_t attr;             // Windows GetFileAttributes
    std::string owner_sid;     // 字面量 SID 字符串
    std::array<uint8_t,32> acl_digest{};  // 全 0 = 未采集
    std::string target_path;   // 链接目标，绝对或相对
};

struct FileEntry {
    std::string rel_path;      // 相对源根的 UTF-8 路径，\\ 分隔
    EntryType type;
    Metadata meta;
    FileTimes times;
    std::optional<uint64_t> inode_id;  // 用于硬链接去重
};

struct BackupOptions {
    std::filesystem::path source_root;
    std::filesystem::path output_pkg;
    bool compress = false;     // 哈夫曼
    bool encrypt  = false;     // AES-256-GCM
    std::optional<std::string> password;
    FilterRules filters;       // 6 类
    uint32_t kdf_iters = 200000;
};

struct RestoreOptions {
    std::filesystem::path pkg;
    std::filesystem::path dest_root;
    std::optional<std::string> password;
    bool overwrite = false;
};

struct BackupProgress {
    uint64_t total_bytes;
    uint64_t done_bytes;
    uint32_t total_files;
    uint32_t done_files;
    std::string current_file;  // 当前正在处理的文件相对路径
    std::vector<std::string> warnings;  // 已确认的告警
};

class BackupTask {
public:
    explicit BackupTask(BackupOptions opt, std::shared_ptr<Logger> log);
    // 同步：阻塞直到完成或抛异常
    void run(std::function<bool(const BackupProgress&)> on_progress = {});
    // 异步：返回 std::future，调用 cancel() 可中断
    std::future<void> run_async(...);
    void cancel();
};

class RestoreTask {
public:
    explicit RestoreTask(RestoreOptions opt, std::shared_ptr<Logger> log);
    void run(std::function<bool(const RestoreProgress&)> on_progress = {});
    void cancel();
};

class ArchiveReader {
public:
    explicit ArchiveReader(const std::filesystem::path& pkg,
                           std::optional<std::string> pwd);
    [[nodiscard]] const FileHeader& header() const;
    [[nodiscard]] std::vector<IndexEntry> read_index();
    // 流式读取：定位到第 i 个条目，返回原始（解密→解压后）字节
    std::vector<uint8_t> read_payload(const IndexEntry& e);
    [[nodiscard]] bool verify_integrity();   // 校验 trailer
};

class ArchiveWriter {
public:
    ArchiveWriter(const std::filesystem::path& pkg, BackupOptions opt);
    void append(const FileEntry& e, std::span<const uint8_t> data);
    void finalize();   // 写 trailer
};
}
```

**强约束：GUI 调用 `BackupTask`/`RestoreTask` 时通过 `on_progress` 回调拿进度。回调返回 `false` 表示用户取消。**

---

## 5. 异常与日志约定

- 所有错误用 `pbackup::BackupError`（继承 `std::runtime_error`），带 `ErrorCode` 枚举：`IOError, PermissionDenied, InvalidPassword, PkgCorrupted, PkgVersionUnsupported, FilterConfigInvalid, Cancelled, Unknown`。
- 日志接口 `Logger`：支持 `info/warn/error` 三级；可同时写文件（`backup.log`）与控制台；线程安全。CLI 默认 `info`，GUI 由调用方注入。
- 日志条目格式：`[2026-07-12 14:33:21.123] [INFO] <模块> 消息`。

---

## 6. 测试要求

`backup_tests` 通过 `ctest` 一键运行，至少 30 个用例（详见 `tests/CMakeLists.txt`），覆盖以下场景，每个场景至少 1 个用例：

- 普通文件 / 多层目录 / 空目录 / 空文件 / 大文件（≥100 MB 临时文件）/ 中文路径 / 只读 / 隐藏 / 系统 / 符号链接 / 硬链接
- 6 类筛选各自的命中与排除
- 压缩开关的备份还原
- 加密备份：合法密码成功、错误密码抛 `InvalidPassword`
- `.pbackup` 字节级被截断/篡改后 `verify` 抛 `PkgCorrupted`
- 目标目录不存在自动创建 / 已存在且 `overwrite=false` 报错 / 已存在且 `overwrite=true` 覆盖
- 源目录不存在 / 权限不足抛 `PermissionDenied`
- 备份包跨会话解压（用固定种子做盐的随机性验证）
- 32 GB 边界不要求，但大文件分块读取要正确
- CLI 子命令解析
- 进度回调被调用次数

---

## 7. 第三方依赖与构建

- 必需：
  - CMake ≥ 3.16
  - C++17 编译器（MSVC v143 / MinGW-w64 11+ / clang 14+）
  - GoogleTest v1.14+（用 FetchContent 拉取，离线场景允许退化到本地 `vendor/googletest`，但默认走 FetchContent）
  - OpenSSL 1.1 或 3.x（Windows 走 vcpkg 或预编译 `C:/OpenSSL-Win64`，CMake 通过 `find_package(OpenSSL REQUIRED)` 找）
- 禁用：
  - 任何 Python/Ruby 等脚本语言做后台逻辑
  - 加密、压缩以外的第三方库"直接"调用（避免扩展分折半）
  - 自行实现对称加密算法（必须用 OpenSSL）

`CMakeLists.txt` 必须能在一行命令下完成构建：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

并在 Windows + VS2022 下用 `cmake -G "Visual Studio 17 2022" -A x64` 也能通过。

---

## 8. AI 训练证书占位（评分需要 +10 分）

在 `src/cli/main.cpp` 顶部以及 `include/pbackup/logger.hpp` 文件头都加入以下注释块（写报告时会原样引用）：

```cpp
// AI-Training-Certificate-Header
// 项目组使用以下 AI 工具辅助开发：
//   - 工具 1：ChatGPT（OpenAI），用于后端核心逻辑（目录扫描、备份包格式、哈夫曼、AES-GCM）
//   - 工具 2：Claude（Anthropic），用于 GUI、文档撰写、UML 校对
//   - 工具 3：StarUML C++ 扩展（自动化），用于正向工程生成类骨架
// 使用方式：需求澄清 → 接口设计 → 单元测试 → 编码实现 → 代码评审，每一步均经组员人工核验。
// 效果评价：加速明显，但所有加密与压缩路径均经过人工逐路径审计与 GoogleTest 覆盖。
```

---

## 9. 不在本 goal 范围内的项

- Qt GUI（前端组实现）
- StarUML 建模（前端组用 `.mdj` 模板生成）
- 实验报告 Word/PDF 撰写（前端组写）
- 答辩 PPT（前端组写）
- 演示视频（人工录制）
- 定时/实时/网络三个扩展项（明确不做）
- 跨平台兼容（仅 Windows；Linux 编译能过即可，不投入测试资源）

---

## 10. 交付物验收清单（GPT 自检用）

GPT 提交前必须自查：

- [ ] `cmake -S . -B build && cmake --build build -j` 在 Windows + VS2022 一次通过
- [ ] `ctest --test-dir build --output-on-failure` 全部通过，至少 30 个用例
- [ ] `backup.exe` 三个子命令可执行
- [ ] `samples/gen_sample.ps1` 能生成含 30+ 各种类型文件的样例树
- [ ] 用该样例树跑一次完整 backup → restore，比对还原目录与原目录字节级一致
- [ ] 加密备份 + 错误密码必须抛 `InvalidPassword`，不能用明文比对绕过
- [ ] `docs/format.md` 写清 §2 的二进制格式
- [ ] 公共头文件 `include/pbackup/*.hpp` 不依赖 Qt 或 GUI
- [ ] 代码注释比例 ≥ 20%（GoogleTest 测试文件除外）
- [ ] 命名规范：`snake_case` 文件名，`PascalCase` 类名，`camelCase` 方法/字段

---

## 11. 风险与注意事项

- Windows 长路径（>260 字符）：统一在内部用 `\\?\` 前缀展开，输出到 GUI 时再回退。
- 硬链接识别依赖 `GetFileInformationByHandle` 的 `nFileIndexHigh/Low`，同卷内 `nFileIndex` 相同视为同一硬链接；`inode_id` 字段保存此值。
- 软链接/Junction 备份内容是目标路径，不跟随递归，避免环。还原时用 `CreateSymbolicLinkW` / `CreateJunction`（后者要走 `FSCTL_SET_REPARSE_POINT`）。
- 哈希校验：CRC32 用于条目与头，SHA-256 用于整包；用 OpenSSL 的 EVP 接口，不要自己实现。
- Qt 在另一进程/线程拉起 `BackupTask` 即可；`backup_core` 必须是线程安全的（至少在 Qt 调用模式下），需要内部互斥或每任务单实例。

---

> **下一步**：GPT 给出 commit-by-commit 提交计划与首批 PR diff；前端组在拿到 `backup_core` 接口稳定版本后开始 Qt GUI 与文档工作。
> **与前端对接**：本 goal 的 §4 是"后端库内部契约"，而前端 GUI 已定稿的对接契约见同目录 `01_codex_prompt.md`（含 `BackendAdapter.h` 真实接口与 `RealBackend` 桥接层要求）。交给 Codex 时以 `01_codex_prompt.md` 为准。
