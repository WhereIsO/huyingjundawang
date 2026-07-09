# 数据备份工具

一个面向 Windows 的桌面数据备份软件，使用 C++17、Qt Widgets、CMake 和 MSVC 构建。项目目标是完成课程实验要求中的数据备份、数据还原，以及文件类型、元数据、自定义筛选、打包解包、压缩解压、加密解密和图形界面扩展功能。

## 功能概览

- 目录备份与备份包还原
- 单文件 `.pbackup` 归档格式
- 普通文件、目录、空目录、硬链接、符号链接、Junction 等文件类型识别
- 文件尺寸、时间、属性、所有者 SID、ACL 摘要等元数据采集
- 路径、类型、名称、时间、尺寸、用户六类筛选
- 哈夫曼压缩与解压
- Windows CNG AES-256-GCM 加密与认证解密
- Qt 图形界面、进度反馈、运行日志、任务历史
- GoogleTest / CTest 自动化测试

## 目录结构

```text
.
├── CMakeLists.txt
├── CMakePresets.json
├── docs/
│   └── format.md
├── src/
│   ├── core/                  # 纯 C++ 后端核心
│   ├── main.cpp
│   ├── MainWindow.*
│   ├── BackupTab.*
│   ├── RestoreTab.*
│   ├── FilterTab.*
│   ├── RealBackend.*
│   └── ...
└── tests/
    ├── core_tests.cpp
    ├── real_backend_smoke.cpp
    └── gui_signal_smoke.cpp
```

`src/core` 不依赖 Qt，负责扫描、筛选、归档、压缩、加密、元数据、备份和还原。Qt 界面通过 `BackendAdapter` 调用 `RealBackend`，双击程序即进入正式备份后端。

## 环境要求

- Windows
- Visual Studio 2022 MSVC 工具链
- CMake 3.16+
- Ninja
- Qt 5.12+ 或 Qt 6，且 Qt ABI 需要与 MSVC 匹配
- GoogleTest，或配置时开启 `BACKUP_ALLOW_GTEST_DOWNLOAD`

## 构建

在 VS 2022 Developer PowerShell 或 Developer Command Prompt 中执行：

```powershell
$env:QT_PREFIX = "<Qt安装目录>"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$env:QT_PREFIX" -DCMAKE_CXX_COMPILER=cl
cmake --build build
```

如果当前源码路径包含中文且 Qt AUTOMOC 出现路径编码问题，可先复制到英文路径再构建。

## 测试

```powershell
ctest --test-dir build --output-on-failure
```

如果本机没有安装 GoogleTest，可在配置时允许 CMake 下载：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$env:QT_PREFIX" -DCMAKE_CXX_COMPILER=cl -DBACKUP_ALLOW_GTEST_DOWNLOAD=ON
```

## 运行

构建完成后运行：

```powershell
.\build\BackupTool.exe
```

程序使用真实备份后端，不需要额外启动脚本。

## 自动化构建与分发

仓库提供 GitHub Actions workflow：

- 普通 push / PR：自动在 Windows 上构建、测试并上传 `BackupTool-windows-x64` artifact。
- 推送 `v*` 标签：自动创建或更新 GitHub Release，并上传 Windows x64 zip 包。

本地和 CI 使用同一个打包脚本：

```powershell
.\scripts\package_windows.ps1 -BuildDir build -Config Release
```

详细说明见：

- `github_actions_usage.md`
- `acceptance_matrix.md`

## 说明

本仓库已整理为单一主线工程，不再保留旧的多版本堆叠目录。历史版本差异可通过 Git 提交记录查看。
