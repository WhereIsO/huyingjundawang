# 当前状态与交接清单

本文档记录当前工程事实，供后续报告、PPT、演示视频和二次开发使用。

## 1. 当前运行入口

项目根目录：

```text
Project_BackupTool
```

真实模式：

```text
启动软件-本地.bat
```

演示模式：

```text
启动软件-仅演示界面.bat
```

自包含运行目录：

```text
bin\BackupTool.exe
bin\Qt5*.dll
bin\platforms\qwindows.dll
```

可分发包：

```text
dist\数据备份工具_可分发.zip
```

当前运行不依赖 Anaconda、固定 `G:` 盘或某台电脑的 Qt 安装目录。

## 2. 当前完成情况

| 项 | 状态 | 证据 |
|---|---|---|
| Qt GUI | 已实现并美化 | `gui/BackupTool/src/MainWindow.*`、`Theme.*`、`BackupTab.*`、`RestoreTab.*`、`FilterTab.*` |
| MockBackend | 已实现 | `BackendAdapter.cpp` |
| RealBackend | 已实现 | `src/RealBackend.*` |
| 纯 C++ core | 已实现 | `src/core/*.h`、`src/core/*.cpp` |
| 前后端联动 | 已实现 | `RealBackend` 调用 `BackupTask` / `RestoreTask` |
| 自包含运行 | 已实现 | `bin` 目录含 exe、Qt/VC 运行库、平台插件 |
| 自动化测试源码 | 44 项 | `gui/BackupTool/tests/*.cpp` |
| `.pbackup` 格式文档 | 已有 | `gui/BackupTool/docs/format.md`、`src/core/docs/format.md` |
| UML 图片 | 已有 | `docs/uml/*.png` |
| GUI 截图 | 已有 | `docs/screenshots/*.png` |
| PPT | 已有 | `ppt/数据备份软件_答辩.pptx` |
| 实验报告 | 已有 | `report/实验报告.md` |

## 3. 本轮修复点

- 筛选页默认不再传出类型筛选，避免空目录、Junction、ReparsePoint 被默认排除。
- 类型筛选补充 `emptydir`、`junction`、`reparse` 选项。
- 主窗口根据 `BACKUP_BACKEND_MODE` 显示真实后端或演示后端，不再固定提示 Mock。
- 增加顶部状态/装饰栏和统一样式表，界面视觉更完整。
- 还原时拒绝包内绝对路径、盘符路径和 `..` 越界路径。
- 新增 `BackupRestoreTest.RejectsPathTraversalInArchive`。
- CMake 默认不再联网下载 GoogleTest，只有 `BACKUP_ALLOW_GTEST_DOWNLOAD=ON` 时允许下载。
- 构建脚本不再写死 `G:`、Anaconda 或固定 VS Community 路径。
- 根目录启动脚本改为使用项目自带 `bin` 运行库。

## 4. 构建说明

源码构建入口：

```bat
cd gui\BackupTool
build_msvc.bat
```

可选环境变量：

```bat
set QT_PREFIX=C:\Qt\5.15.2\msvc2019_64
set BACKUP_ALLOW_GTEST_DOWNLOAD=ON
set RUN_TESTS=1
```

默认镜像目录：

```text
%TEMP%\BackupTool_src
```

默认构建目录：

```text
%TEMP%\BackupTool_build
```

## 5. 测试事实

当前测试源码中共有 44 项 `TEST(...)`：

- 41 项 core 单元测试。
- 2 项 RealBackend smoke test。
- 1 项 GUI 信号 smoke test。

历史记录曾验证 43 项通过、0 失败、2 项 symlink 权限相关测试跳过。本轮新增第 44 项路径穿越防护测试后，尚需在具备 Qt/MSVC/GTest 的机器上重新运行 CTest 并更新报告截图。

## 6. 已知限制

- 未实现定时备份、实时备份、网络备份。
- 未知 ReparsePoint 按目录尽力恢复并记录 warning。
- ACL 当前保存摘要，完整 ACL 还原按权限尽力而为。
- 直接双击 `BackupTool.exe` 默认仍是 Mock；真实备份请使用真实模式脚本。
- 从源码构建仍需要 Qt 开发包和匹配 ABI 的 MSVC/Qt 组合。

## 7. 后续建议

- 在当前机器补跑 CTest，更新“43 项”相关报告描述为新的实际结果。
- 重新截图 GUI，替换旧截图以展示新主题。
- 重新打包 `dist/数据备份工具_可分发.zip`，确保分发包包含最新 exe。
