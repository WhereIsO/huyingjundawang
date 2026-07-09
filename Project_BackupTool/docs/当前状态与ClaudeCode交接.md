# 当前状态与 Claude Code 交接清单

本文档只记录当前工程事实，不写宣传性措辞。后续若交给 Claude Code 加工报告、PPT、UML 或演示文稿，应优先以本文档为事实来源。

## 1. 重要路径

项目根目录：

```text
F:\Files\Payki in UESTC\学习与课程\VII. Senior I（2026-2027）\【工】软件开发综合实验-李忻洋\Project_BackupTool
```

GUI 与后端源码：

```text
F:\Files\Payki in UESTC\学习与课程\VII. Senior I（2026-2027）\【工】软件开发综合实验-李忻洋\Project_BackupTool\gui\BackupTool
```

ASCII 镜像源码：

```text
G:\_src_backup_tool_gui
```

构建目录：

```text
G:\_build_backup_tool_gui_ascii
```

当前可执行程序：

```text
G:\_build_backup_tool_gui_ascii\BackupTool.exe
```

## 2. 当前完成情况

| 项 | 状态 | 证据 |
|---|---|---|
| Qt GUI | 已实现 | `gui/BackupTool/src/MainWindow.*`、`BackupTab.*`、`RestoreTab.*`、`FilterTab.*` |
| MockBackend | 已实现 | `BackendAdapter.cpp` |
| RealBackend | 已实现 | `src/RealBackend.h`、`src/RealBackend.cpp` |
| 纯 C++ core | 已实现 | `src/core/*.h`、`src/core/*.cpp` |
| 前后端联动 | 已验证 | `real_backend_smoke` 真实备份→还原通过 |
| GUI 信号更新 | 已验证 | `gui_signal_smoke` 通过 |
| 自动化测试 | 已验证 | CTest 43 项，0 失败 |
| `.pbackup` 格式文档 | 已有 | `gui/BackupTool/docs/format.md`、`src/core/docs/format.md` |
| core README | 已有 | `src/core/README.md` |
| 使用说明 | 已新增 | `docs/使用说明.md` |
| 开发流程 | 已新增 | `docs/前后端开发流程.md` |
| 实验报告 | 已更新部分事实 | `report/实验报告.md` |

## 3. 真实运行方式

真实后端启动命令：

```powershell
$env:BACKUP_BACKEND_MODE = "real"
$env:Path = "G:\Anaconda3\Library\bin;$env:Path"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "G:\Anaconda3\Library\plugins\platforms"
& "G:\_build_backup_tool_gui_ascii\BackupTool.exe"
```

默认不设置 `BACKUP_BACKEND_MODE` 时使用 MockBackend。

## 4. 最近一次测试事实

最近一次 `build_msvc.bat` 构建与测试结果：

```text
100% tests passed, 0 tests failed out of 43
```

跳过测试：

```text
BackupRestoreTest.SymlinkRoundTripWhenSupported
ScannerTest.DetectsSymlinkWhenSupported
```

跳过原因：当前 Windows 环境不允许创建符号链接。这是权限/系统设置问题，不是构建失败。

测试目标：

| target | 数量 | 内容 |
|---|---:|---|
| `backup_tests` | 40 | core 单元测试 |
| `real_backend_smoke` | 2 | RealBackend 工厂切换、真实备份还原 |
| `gui_signal_smoke` | 1 | GUI 进度和日志槽更新 |

## 5. 43 项测试清单

1. `HuffmanTest.EmptyRoundTrip`
2. `HuffmanTest.TextRoundTrip`
3. `HuffmanTest.SingleSymbolRoundTrip`
4. `HuffmanTest.BinaryRoundTrip`
5. `HuffmanTest.RejectInvalidHeader`
6. `CryptoTest.SaltHasEightBytes`
7. `CryptoTest.EncryptDecryptRoundTrip`
8. `CryptoTest.WrongPasswordFails`
9. `CryptoTest.PackUnpackEncryptedPayload`
10. `CryptoTest.EmptyPasswordRejected`
11. `FilterTest.IncludePathMatches`
12. `FilterTest.ExcludePathRejects`
13. `FilterTest.NameGlobMatches`
14. `FilterTest.TypeFilterMatches`
15. `FilterTest.SizeRangeMatches`
16. `FilterTest.OwnerMatches`
17. `FilterTest.DateParserAcceptsValidDate`
18. `FilterTest.BadTypeFilterThrows`
19. `ArchiveTest.PlainArchiveRoundTrip`
20. `ArchiveTest.CompressedArchiveRoundTrip`
21. `ArchiveTest.EncryptedArchiveRoundTrip`
22. `ArchiveTest.EncryptedArchiveWrongPasswordFails`
23. `ArchiveTest.CorruptedArchiveFails`
24. `BackupRestoreTest.OrdinaryFileRoundTrip`
25. `BackupRestoreTest.NestedDirectoryRoundTrip`
26. `BackupRestoreTest.EmptyFileRoundTrip`
27. `BackupRestoreTest.EmptyDirectoryRoundTrip`
28. `BackupRestoreTest.ChinesePathRoundTrip`
29. `BackupRestoreTest.NameFilterExcludesNonMatchingFiles`
30. `BackupRestoreTest.EncryptedBackupWrongPasswordFails`
31. `BackupRestoreTest.OverwriteFalseRejectsExistingFile`
32. `BackupRestoreTest.OverwriteTrueReplacesExistingFile`
33. `BackupRestoreTest.SymlinkRoundTripWhenSupported`（当前环境跳过）
34. `BackupRestoreTest.JunctionRoundTripWhenSupported`
35. `ScannerTest.ScansRegularFiles`
36. `ScannerTest.DetectsHardlinkWhenSupported`
37. `ScannerTest.DetectsSymlinkWhenSupported`（当前环境跳过）
38. `ScannerTest.DetectsJunctionWhenSupported`
39. `MetadataTest.AttributesRoundTripReadonly`
40. `ProgressTest.BackupInvokesProgressCallback`
41. `RealBackendSmokeTest.FactoryReturnsRealBackendWhenEnvSet`
42. `RealBackendSmokeTest.BackupThenRestoreRoundTrip`
43. `GuiSignalSmokeTest.ProgressAndLogSlotsUpdateWidgets`

## 6. 源码质量事实

core 源码统计：

```text
文件数：22
非空行：2494
注释行：572
注释比例：约 22.94%
```

说明：统计范围是 `gui/BackupTool/src/core` 下的 `.h` 和 `.cpp` 文件，不含测试文件。

## 7. 当前实现的扩展功能

| 扩展 | 状态 | 说明 |
|---|---|---|
| 文件类型支持 | 已实现 | 普通文件、目录、空目录、硬链接、符号链接、Junction；未知 ReparsePoint 尽力恢复 |
| 元数据支持 | 已实现 | 大小、属性、创建/访问/修改时间、Owner SID、ACL 摘要 |
| 自定义备份 | 已实现 | 路径、类型、名称、时间、尺寸、用户 |
| 打包解包 | 已实现 | `.pbackup` 单文件归档 |
| 压缩解压 | 已实现 | 自研哈夫曼 |
| 加密解密 | 已实现 | Windows CNG AES-256-GCM |
| 图形界面 | 已实现 | Qt 5 Widgets |
| 定时备份 | 未实现 | 明确不做 |
| 实时备份 | 未实现 | 明确不做 |
| 网络备份 | 未实现 | 明确不做 |

## 8. 重要工程决策

1. 使用 MSVC，不使用 MinGW。原因：本机 Qt 是 MSVC 构建，ABI 不兼容 MinGW。
2. 使用 Windows CNG，不使用 OpenSSL。原因：减少部署复杂度，并满足加密必须使用成熟 API 的要求。
3. 压缩使用自研 Huffman。原因：课程要求压缩不能直接调用第三方库。
4. core 不依赖 Qt。原因：便于单元测试和职责分离。
5. GUI 通过 `BackendAdapter` 调用后端。原因：前后端解耦。
6. 使用 ASCII 镜像路径构建。原因：Qt AUTOMOC 在中文路径下存在 include 路径问题。
7. Junction 还原使用 `FSCTL_SET_REPARSE_POINT`。原因：不能只按普通目录恢复，否则文件类型支持不完整。

## 9. 已知限制与后续加工点

必须由后续文档/PPT/视频阶段补齐：

- 学生姓名、学号。
- StarUML `.mdj` 模型和导出的图。
- 需求用例图、类图、顺序图、状态图的正式图片。
- GUI 截图。
- AI 工具使用截图。
- PPT。
- 演示视频。

当前 `docs/uml`、`docs/screenshots`、`docs/figures` 目录为空。报告中如果引用这些图，需要 Claude Code 或人工后续生成并放入对应目录。

## 10. 不要误写的点

- 不要写“真实后端待集成”，真实后端已经集成。
- 不要写“32 条测试”，当前是 43 项 CTest。
- 不要写“13/13 构建”，当前事实应写 CTest 43 项通过。
- 不要写“定时/实时备份视进度”，应写本期不做。
- 不要写“OpenSSL 加密”，当前实现是 Windows CNG。
- 不要写“项目已经有 Git 记录”，当前项目目录不是 Git 仓库。
- 不要写“Junction 只能按目录恢复”，当前 Junction 已能真实恢复；未知 ReparsePoint 才按目录尽力恢复。

