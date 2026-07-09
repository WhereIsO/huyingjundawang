# 实验要求验收矩阵

状态：基于当前 `cleanup-mainline-demo79` 分支整理。
说明：本矩阵用于对照课程资料和项目总要求，不替代最终实验报告、测试报告和答辩材料。

## 功能要求

| 要求 | 当前实现 | 状态 | 证据与备注 |
|---|---|---|---|
| 数据备份 | GUI 发起 `BackupTask`，扫描目录并写入 `.pbackup` | 已完成 | `src/core/backup_task.cpp`、`tests/core_tests.cpp` |
| 数据还原 | GUI 发起 `RestoreTask`，从 `.pbackup` 恢复目录树 | 已完成 | `src/core/backup_task.cpp`、`tests/core_tests.cpp` |
| 文件类型支持 | 普通文件、目录、空目录、符号链接、硬链接、Junction、ReparsePoint | 基本完成 | Windows 权限不足时符号链接测试会跳过；命名管道/设备对象需在报告中说明不作为普通目录树文件处理 |
| 元数据支持 | 保存大小、属性、时间戳、Owner SID、ACL 摘要、链接目标、硬链接标识 | 部分完成 | 还原阶段当前主要恢复时间戳和属性；Owner/ACL 完整恢复未实现，报告中需说明权限限制和当前口径 |
| 自定义备份：路径 | 包含路径、排除路径 | 已完成 | `src/core/filter.cpp`、`src/FilterTab.cpp` |
| 自定义备份：类型 | 文件、目录、符号链接、硬链接等类型筛选 | 已完成 | `parseTypeFilter`、GUI 类型复选框 |
| 自定义备份：名称 | glob 文件名筛选 | 已完成 | `nameGlob` |
| 自定义备份：时间 | 修改时间范围筛选 | 已完成 | `mtimeAfterNs`、`mtimeBeforeNs` |
| 自定义备份：尺寸 | 最小/最大文件大小筛选 | 已完成 | `sizeMin`、`sizeMax` |
| 自定义备份：用户 | Owner SID 筛选 | 已完成 | `ownerSid` |
| 打包解包 | 单文件 `.pbackup` 归档格式 | 已完成 | `src/core/archive.*`、`src/core/docs/format.md` |
| 压缩解压 | 自实现哈夫曼压缩/解压 | 已完成 | `src/core/huffman.*`、Huffman 单元测试 |
| 加密解密 | Windows CNG AES-256-GCM，PBKDF2-SHA256 派生密钥 | 已完成 | `src/core/crypto.*`，未手写对称加密算法 |
| 图形界面 | Qt Widgets GUI，含备份、还原、筛选、日志、进度 | 已完成 | `src/MainWindow.cpp`、`src/BackupTab.cpp`、`src/RestoreTab.cpp`、`src/FilterTab.cpp` |
| 定时备份 | 未纳入本项目必做范围 | 不做 | 报告需说明范围选择 |
| 实时备份 | 未纳入本项目必做范围 | 不做 | 报告需说明范围选择 |
| 网络备份 | 未纳入本项目必做范围 | 不做 | 报告需说明范围选择 |

## 工程与源码质量

| 要求 | 当前实现 | 状态 | 证据与备注 |
|---|---|---|---|
| Windows + C++ | C++17，Windows API，Qt Widgets | 已完成 | `CMakeLists.txt` |
| 后台禁止 Python | 后端核心为 C++ | 已完成 | CI 使用 Python 仅安装 Qt，不参与程序后台逻辑 |
| CMake 自动化构建 | 顶层 `CMakeLists.txt` | 已完成 | 支持本地构建和 GitHub Actions 构建 |
| 源码目录合理 | `src/`、`src/core/`、`tests/`、`docs/` | 基本完成 | 不是旧 goal 中 `include/pbackup` + CLI 结构，但符合当前 GUI 工程 |
| 代码命名规范 | 类名 PascalCase，核心文件多为 snake_case | 基本完成 | Qt GUI 文件沿用 PascalCase 文件名 |
| 注释比例 20% 以上 | 核心文件头部和关键逻辑有注释 | 待量化 | 需要后续用脚本或人工统计形成报告证据 |
| 白盒单元测试 | GoogleTest + CTest | 已完成 | 当前 CTest 44 项，0 失败，符号链接相关 2 项按环境跳过 |
| 第三方测试工具 | GoogleTest | 已完成 | `tests/` |
| 测试用例超过 30 个 | 当前 44 项 CTest | 已完成 | `ctest` 输出 |
| 测试种类超过 4 种 | 单元、集成、异常、安全、边界、GUI smoke | 基本完成 | 测试报告仍需把种类和用例整理成规范表格 |
| GitHub Actions | Windows 构建、测试、打包、Release workflow | 已补充，待云端首次运行验证 | `.github/workflows/windows-build-release.yml` |
| Release 打包 | `windeployqt` 收集运行时并压缩 | 已补充 | `scripts/package_windows.ps1` |

## 文档与答辩交付

| 要求 | 当前实现 | 状态 | 备注 |
|---|---|---|---|
| README | 项目说明、构建、测试、运行 | 已完成 | 仍可在最终提交前加截图 |
| 备份包格式文档 | `.pbackup` 格式说明 | 已完成 | `src/core/docs/format.md`、`docs/format.md` |
| 需求分析说明书 | 尚未按模板写成最终版 | 未完成 | 需要用例图、用例描述、交互方式、甘特图 |
| 系统设计文档 | 尚未按模板写成最终版 | 未完成 | 需要构件图、类图、顺序图、核心类说明 |
| 软件测试报告 | 尚未按模板写成最终版 | 未完成 | 需要 30+ 规范测试用例表和测试结果分析 |
| UML 图 | 当前仓库未包含最终 StarUML 图 | 未完成 | 至少用例图、类图、顺序图、构件图 |
| PPT | 当前仓库未包含最终答辩 PPT | 未完成 | 需要单独制作 |
| 演示视频链接 | 当前仓库未包含 | 未完成 | 需要最终录制 |
| AI 开发工具使用总结 | 当前仓库未包含最终报告章节 | 未完成 | 需要截图和效果评价 |
| AI 训练证书 | 根目录保留了证书图片在课程目录，不在仓库 | 已具备材料 | 最终报告需引用 |

## 当前主要风险

1. Owner/ACL 元数据仅记录，未完整还原，报告中需要清楚说明。
2. 符号链接和 Junction 依赖 Windows 权限，演示前要在目标机器上提前验证。
3. GitHub Actions 首次运行会下载 Qt 和 GoogleTest，耗时较长。
4. 最终交付不能只交源码，还要交可执行 Release 包、报告、PPT 和演示视频链接。
