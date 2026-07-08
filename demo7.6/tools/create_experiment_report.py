from __future__ import annotations

import copy
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET


TEMPLATE = Path(
    "C:/APP/WeChat/File/WeChat Files/wxid_oc7nhkkx33e122/FileStorage/File/2026-07/"
    "\u5c0f\u7ec4\u63d0\u4ea4\u6587\u6863-\u5b9e\u9a8c\u62a5\u544a\u6a21\u677f.docx"
)
OUTPUT = Path("docs/实验报告-文件备份软件.docx")

W_NS = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
CP_NS = "http://schemas.openxmlformats.org/package/2006/metadata/core-properties"
DC_NS = "http://purl.org/dc/elements/1.1/"
DCTERMS_NS = "http://purl.org/dc/terms/"

ET.register_namespace("w", W_NS)
ET.register_namespace("cp", CP_NS)
ET.register_namespace("dc", DC_NS)
ET.register_namespace("dcterms", DCTERMS_NS)


def qn(ns: str, tag: str) -> str:
    return f"{{{ns}}}{tag}"


def w(tag: str) -> str:
    return qn(W_NS, tag)


def split_paragraphs(text: str) -> list[str]:
    lines: list[str] = []
    for raw in text.strip().splitlines():
        line = raw.rstrip()
        if line:
            lines.append(line)
        elif lines and lines[-1] != "":
            lines.append("")
    return lines


def make_paragraph(text: str) -> ET.Element:
    p = ET.Element(w("p"))
    r = ET.SubElement(p, w("r"))
    t = ET.SubElement(r, w("t"))
    t.set(qn("http://www.w3.org/XML/1998/namespace", "space"), "preserve")
    t.text = text
    return p


def set_cell_text(cell: ET.Element, text: str) -> None:
    tc_pr = cell.find(w("tcPr"))
    saved_tc_pr = copy.deepcopy(tc_pr) if tc_pr is not None else None
    for child in list(cell):
        cell.remove(child)
    if saved_tc_pr is not None:
        cell.append(saved_tc_pr)
    for paragraph in split_paragraphs(text):
        cell.append(make_paragraph(paragraph))
    if not split_paragraphs(text):
        cell.append(make_paragraph(""))


def remove_fixed_row_height(row: ET.Element) -> None:
    tr_pr = row.find(w("trPr"))
    if tr_pr is None:
        return
    for height in list(tr_pr.findall(w("trHeight"))):
        tr_pr.remove(height)


def cells_of(row: ET.Element) -> list[ET.Element]:
    return row.findall(w("tc"))


def table_texts() -> dict[int, list[str]]:
    return {
        0: [
            "小组：待填写",
            "组长（学号）、组员（学号）、组员（学号）：待填写",
        ],
        1: ["组长姓名：待填写", "学号：待填写"],
        2: ["组员姓名：待填写", "学号：待填写"],
        3: ["组员姓名：待填写", "学号：待填写"],
        4: ["一、实验室名称：计算机科学与工程学院软件开发综合实验室"],
        5: ["二、实验项目名称：文件备份软件"],
        6: [
            """三、实验目的：
本实验围绕“数据备份软件”这一综合性工程项目，完成从需求分析、系统设计、编码实现到软件测试的完整软件生命周期实践。通过本项目，训练学生综合运用程序设计、数据结构与算法、软件工程、操作系统文件系统接口、软件测试与项目文档编写等知识，提升复杂工程问题的分析、设计、实现、验证和总结能力。
本项目的具体目标包括：
（1）理解数据备份软件的基本业务场景，能够把目录树中的文件数据保存到指定位置，并能够按需恢复到目标目录。
（2）掌握归档格式设计、文件遍历、文件读写、元数据保存、压缩、加密、校验等关键技术。
（3）按照软件工程方法完成需求规格说明、系统设计、测试报告和答辩材料。
（4）实现基本可用、易用、具备健壮性保护的数据备份软件，并通过自动化测试验证主要功能。"""
        ],
        7: [
            """四、实验内容：
设计并实现一款数据备份软件。项目以 C++17 为主要开发语言，在 Windows/MinGW 环境下完成命令行程序和图形界面程序两个入口，并复用同一套备份核心逻辑。
本项目完成的基础要求：
（1）数据备份：递归扫描源目录，将目录树中的普通文件、子目录和空目录保存到指定 `.sdb` 归档文件。
（2）数据还原：读取 `.sdb` 归档文件，将文件内容、目录结构和部分元数据恢复到指定目录。
本项目完成的扩展要求：
（1）元数据支持：保存并尽量恢复 Windows 文件属性和最后修改时间。
（2）自定义备份：支持按扩展名包含、扩展名排除、文件名包含、最小大小、最大大小、修改时间进行筛选。
（3）打包解包：将所有备份条目写入单个 `.sdb` 归档文件。
（4）压缩解压：实现内置 RLE 压缩算法，压缩有效时保存压缩载荷。
（5）加密解密：用户指定密码后，对文件载荷进行加密保存，还原时校验密码和数据校验和。
（6）图形界面：实现友好易用的 Windows GUI，提供 Backup、Restore、Archive List 三个页签，支持目录/文件浏览、筛选输入、密码输入、覆盖选项和状态提示。"""
        ],
        8: [
            """五、实验器材（设备、元器件）：
（1）PC 一台，Windows 操作系统。
（2）MinGW g++ 8.1.0 编译器。
（3）mingw32-make 构建工具。
（4）PowerShell 测试脚本运行环境。
（5）VSCode/Codex 等开发辅助环境。"""
        ],
        9: ["六、实验步骤及操作："],
        10: [
            """需求分析说明书（10分）

1. 任务概述
本软件命名为 Simple Data Backup，简称 sdb，是一款面向个人用户和小型项目组的文件备份软件。用户可以将指定目录保存为单个归档文件，并在需要时恢复到指定目录。软件同时提供命令行入口 `sdb.exe` 和图形界面入口 `sdb_gui.exe`。

1.1 引言
在日常学习和软件开发过程中，源代码、文档、配置文件等数据经常需要备份和迁移。直接复制目录虽然简单，但缺少统一归档、密码保护、筛选和校验能力。本项目通过自定义 `.sdb` 归档格式实现目录树备份，并补充压缩、加密、元数据和 GUI 功能，以满足实验对正确性、易用性和健壮性的要求。

1.2 综合描述
1.2.1 产品状况
本项目是一个新型、自主实现的课程实验项目，不依赖第三方备份工具完成核心功能。命令行程序和 GUI 程序均调用同一套 C++ 业务接口，避免不同入口之间出现功能不一致。

1.2.2 产品功能
FR-01 数据备份：递归扫描源目录，保存普通文件、子目录和空目录。
FR-02 数据还原：从 `.sdb` 归档中恢复目录结构和文件内容。
FR-03 归档查看：显示归档内条目的类型、路径、原始大小、存储大小、修改时间和扩展标记。
FR-04 元数据支持：保存并恢复 Windows 文件属性和最后修改时间。
FR-05 自定义筛选：支持扩展名包含/排除、文件名包含、大小范围、修改时间筛选。
FR-06 打包解包：所有条目写入单个归档文件，便于保存和传输。
FR-07 压缩解压：使用 RLE 算法压缩重复字节较多的文件。
FR-08 加密解密：用户指定密码后加密文件载荷，还原前校验密码。
FR-09 图形界面：通过可视化界面完成备份、还原和归档查看。
FR-10 健壮性保护：拒绝 unsafe 路径、错误密码、损坏归档和默认覆盖已有文件。

1.2.3 用户类和特性
普通用户：需要完成日常目录备份、还原和查看归档内容，偏好易理解的 GUI 操作。
熟练用户/开发者：需要通过命令行批处理执行备份、还原和测试，关注参数明确性和可脚本化能力。
项目维护者：需要阅读源码、运行测试、修改功能和生成实验文档。

1.3 运行环境
硬件平台：普通 PC。
操作系统：Windows。
编译环境：MinGW g++ 8.1.0，C++17。
构建工具：mingw32-make。
运行依赖：Windows 系统 API、通用控件库 comctl32、文件对话框库 comdlg32、shell32、ole32。
内存和磁盘：根据备份文件大小而定；当前实现按文件读取载荷，小规模课程实验数据可正常运行。
网络：单机模式不依赖网络。

2. 功能需求
2.1 功能划分
系统划分为备份模块、还原模块、归档查看模块、筛选模块、压缩模块、加密模块、元数据模块、命令行接口模块、图形界面模块和自动化测试模块。

2.2 系统用例
用例 UC-01：创建备份
参与者：普通用户。
前置条件：源目录存在，归档输出路径合法。
基本流程：用户选择源目录和归档文件；用户设置压缩、密码和筛选条件；系统扫描目录；系统写入归档头、条目元数据和文件载荷；系统输出统计结果。
异常流程：源目录不存在时报错；归档文件位于源目录内部时报错；文件读取失败时终止并提示原因。
后置条件：生成 `.sdb` 归档文件。

用例 UC-02：还原备份
参与者：普通用户。
前置条件：归档文件存在，目标目录可创建。
基本流程：用户选择归档和目标目录；输入密码和覆盖选项；系统读取归档头；系统校验密码；系统依次解密、解压、校验并写入文件；系统恢复元数据。
异常流程：密码错误、校验和不一致、目标文件已存在且未选择覆盖时停止并提示。
后置条件：目标目录中出现还原后的文件和目录结构。

用例 UC-03：查看归档
参与者：普通用户。
前置条件：归档文件存在且格式正确。
基本流程：用户选择归档文件；系统读取条目元数据；命令行输出表格或 GUI 表格展示结果。
后置条件：用户了解归档内容。

用例 UC-04：使用 GUI 操作
参与者：普通用户。
前置条件：`bin/sdb_gui.exe` 可运行。
基本流程：用户在 Backup、Restore、Archive List 页签中选择对应操作；系统通过浏览按钮收集路径，通过复选框和输入框收集选项；操作完成后在状态框和消息框中展示结果。
后置条件：用户无需记忆命令行参数即可完成常见备份任务。

3. 外部接口需求
3.1 用户界面
命令行界面：
`sdb backup <source_dir> <archive.sdb> [options]`
`sdb restore <archive.sdb> <target_dir> [options]`
`sdb list <archive.sdb>`
图形界面：
Backup 页签包含源目录、归档文件、压缩、密码、扩展名筛选、名称筛选、大小筛选、修改时间筛选和开始备份按钮。
Restore 页签包含归档文件、目标目录、密码、覆盖已有文件和还原按钮。
Archive List 页签包含归档文件选择、加载按钮和条目列表表格。

3.2 硬件接口
软件不直接访问特殊硬件，仅通过操作系统文件系统接口访问磁盘文件。

3.3 软件接口
操作系统接口：Win32 API 用于目录遍历、目录创建、文件属性、文件时间和 GUI 控件。
构建接口：Makefile 调用 g++ 生成 `bin/sdb.exe` 和 `bin/sdb_gui.exe`。
测试接口：PowerShell 脚本 `tests/run_tests.ps1` 调用命令行程序进行端到端测试。

4. 其它非功能性需求
4.1 性能需求
能够处理多级目录和多个普通文件；备份和还原过程中应输出统计信息。当前实现适合课程实验和中小规模目录，后续可改为流式分块处理以支持超大文件。
4.2 安全性需求
加密归档必须提供正确密码才能还原；还原时拒绝绝对路径、空路径、`.` 和 `..`，防止归档路径逃逸；默认不覆盖已有文件，必须显式选择覆盖。
4.3 软件质量属性
正确性：还原后的文件内容必须与原文件一致。
易用性：同时提供 CLI 和 GUI。
健壮性：错误输入和异常归档应给出明确提示。
可维护性：接口层和核心业务逻辑分离。

5. 项目规划
人员分工：
项目经理/组长：总体方案、需求分析、进度控制、文档汇总。
开发成员一：归档格式、备份还原、压缩加密、元数据处理。
开发成员二：GUI、测试脚本、答辩材料、演示脚本。
当前报告中的姓名和学号请由小组提交前按实际情况填写。"""
        ],
        11: [
            """系统设计文档（20分）

1. 开发环境和工具
硬件平台：普通 PC。
操作系统：Windows。
编译环境：MinGW g++ 8.1.0，C++17。
IDE/编辑器：VSCode / Codex 桌面环境。
构建工具：mingw32-make。
测试工具：PowerShell 自动化脚本。
依赖库：不使用第三方备份库；GUI 使用 Windows 原生 Win32 控件和系统库 comctl32、comdlg32、shell32、ole32。

2. 总体设计
2.1 系统结构设计
系统采用“接口层 + 业务层 + 归档层 + 文件系统适配层”的结构。
顶层结构：
`sdb.exe` 命令行入口 -> `backup.hpp` 业务接口 -> `backup.cpp` 归档与文件系统逻辑。
`sdb_gui.exe` 图形界面入口 -> `backup.hpp` 业务接口 -> `backup.cpp` 归档与文件系统逻辑。
命令行入口和 GUI 入口共享核心接口，保证两种使用方式行为一致。

2.2 主要组件说明
命令行接口层：`src/main.cpp`，负责解析 `backup`、`restore`、`list` 命令和参数。
图形界面层：`src/gui.cpp`，负责创建 Backup、Restore、Archive List 三个页签，处理按钮事件、文件/目录浏览、状态提示。
业务接口层：`src/backup.hpp`，定义 `BackupOptions`、`RestoreOptions`、`OperationStats`、`ArchiveEntryInfo` 和业务函数。
归档实现层：`src/backup.cpp`，实现 `.sdb` 文件头、条目元数据、载荷读写、压缩、加密、校验和恢复。
文件系统适配层：使用 Win32 API 完成目录递归遍历、路径规范化、目录创建、文件属性和修改时间处理。

3. 静态建模
3.1 系统对象模型
核心数据结构包括：
FilterOptions：保存筛选条件，包括扩展名、名称、大小、修改时间。
BackupOptions：保存备份选项，包括是否压缩、密码和筛选条件。
RestoreOptions：保存还原选项，包括密码和是否覆盖已有文件。
OperationStats：保存操作统计信息，包括目录数、文件数、原始字节数、存储字节数。
ArchiveEntryInfo：用于归档查看，保存条目类型、路径、大小、修改时间和压缩/加密标记。
ArchiveHeader：内部结构，保存归档版本、标记、创建时间、密码指纹和条目数量。
EntryMeta：内部结构，保存每个条目的元数据和校验和。
PendingEntry：内部结构，保存扫描阶段待备份条目。

3.2 类/结构描述
FilterOptions：
属性：includeExtensions、excludeExtensions、nameContains、minSize、maxSize、hasModifiedAfter、modifiedAfter。
用途：描述用户自定义备份规则。

BackupOptions：
属性：compress、password、filter。
用途：描述一次备份任务的全部用户选项。

RestoreOptions：
属性：password、overwrite。
用途：描述一次还原任务的用户选项。

OperationStats：
属性：directories、files、originalBytes、storedBytes。
用途：向 CLI/GUI 返回操作结果统计。

ArchiveEntryInfo：
属性：path、type、originalSize、storedSize、modifiedTime、compressed、encrypted。
用途：归档查看功能的数据传输对象。

4. 动态建模
4.1 场景：创建备份
步骤：
1）用户通过 CLI 或 GUI 输入源目录、归档路径和选项。
2）系统规范化路径，检查源目录是否存在，并拒绝将归档放在源目录内部。
3）系统递归扫描目录树，跳过 reparse point，防止目录链接循环。
4）系统按筛选规则选择普通文件，同时保存目录条目。
5）系统写入归档头。
6）对每个文件读取内容，计算 FNV-1a 校验和；若启用压缩则尝试 RLE 压缩；若设置密码则加密载荷。
7）系统写入条目元数据和载荷，返回统计信息。

4.2 场景：还原备份
步骤：
1）用户通过 CLI 或 GUI 输入归档路径、目标目录和选项。
2）系统读取并校验归档 magic 和版本。
3）若归档加密，系统检查用户密码指纹。
4）系统逐条读取元数据，拒绝绝对路径和 `..` 路径。
5）目录条目先创建；文件条目读取载荷后解密、解压、校验。
6）系统写入文件，恢复文件属性和修改时间。
7）所有文件完成后，按路径长度逆序恢复目录元数据。

4.3 场景：GUI 备份
步骤：
1）用户在 Backup 页签点击 Browse 选择源目录和归档文件。
2）用户勾选 Compress、输入 Password 和筛选条件。
3）用户点击 Start Backup。
4）GUI 组装 BackupOptions 并调用 createBackup。
5）结果显示在状态框和消息框中。

5. 数据库设计
本项目不使用数据库。持久化数据采用自定义 `.sdb` 二进制归档格式。
归档文件头字段：magic、version、flags、createdAt、passwordFingerprint、entryCount。
条目字段：type、flags、path、originalSize、storedSize、modifiedTime、permissions、checksum、payload。
整数采用小端序写入，避免结构体内存布局差异。"""
        ],
        12: [
            """软件测试报告（20分）

1. 引言
测试目标是验证数据备份软件的核心功能、扩展功能和错误处理能力，确保软件满足实验要求。测试采用自动化端到端脚本和构建产物检查相结合的方式进行。
测试环境：
操作系统：Windows。
编译器：MinGW g++ 8.1.0。
构建工具：mingw32-make。
测试脚本：`tests/run_tests.ps1`。

2. 功能测试
测试用例 TC-01：构建测试
预置条件：源码、Makefile 存在。
操作步骤：执行 `mingw32-make`。
预期结果：生成 `bin/sdb.exe` 和 `bin/sdb_gui.exe`。
实际结果：通过。

测试用例 TC-02：加密压缩备份
预置条件：测试脚本创建包含多级目录、文本文件、二进制样例文件、空目录和 `.tmp` 文件的源目录。
操作步骤：执行 `sdb.exe backup <source> <archive> --compress --password secret --exclude-ext .tmp`。
预期结果：生成 `.sdb` 归档，`.tmp` 文件被排除。
实际结果：通过。

测试用例 TC-03：归档查看
操作步骤：执行 `sdb.exe list <archive>`。
预期结果：输出包含 `docs/readme.txt` 等条目。
实际结果：通过。

测试用例 TC-04：正确密码还原
操作步骤：执行 `sdb.exe restore <archive> <restore> --password secret`。
预期结果：文件和空目录恢复成功。
实际结果：通过。

测试用例 TC-05：内容一致性
操作步骤：比较源文件和还原文件内容。
预期结果：文本文件和二进制样例文件内容一致。
实际结果：通过。

测试用例 TC-06：筛选规则
操作步骤：检查还原目录中是否存在 `docs/skip.tmp`。
预期结果：由于使用 `--exclude-ext .tmp`，该文件不应还原。
实际结果：通过。

测试用例 TC-07：错误密码
操作步骤：使用错误密码执行还原。
预期结果：程序返回失败，提示 wrong password。
实际结果：通过。

测试用例 TC-08：GUI 构建产物
操作步骤：检查 `bin/sdb_gui.exe` 是否存在。
预期结果：GUI 程序成功生成。
实际结果：通过。

3. 代码测试
代码按模块划分为命令行入口、GUI 入口、备份核心和测试脚本。关键检查点包括：
（1）命令行参数缺失或错误时给出错误提示。
（2）归档文件不能位于源目录内部，避免递归包含自身。
（3）还原路径必须为安全相对路径，拒绝绝对路径、`.` 和 `..`。
（4）加密归档必须校验密码指纹。
（5）还原后使用校验和验证内容正确性。

4. 性能测试
当前自动化测试使用小规模样例目录，主要验证功能正确性。设计上单个文件读取到内存后处理，适合课程实验和中小规模目录。对于超大文件，后续可改为分块流式读写以降低内存占用。

5. 健壮性测试
覆盖场景包括：错误密码、目标文件默认不覆盖、排除文件不还原、归档路径安全检查、空目录恢复。测试结果表明主要异常路径能够给出错误提示或正确处理。

6. 测试结果分析
执行命令：
`mingw32-make test`
实际输出关键内容：
Backup created: ...sample.sdb
Directories: 2
Files: 2
Original bytes: 150
Stored bytes: 20
Archive restored to: ...restore
Directories: 2
Files: 2
Restored bytes: 150
All tests passed.

结论：自动化测试覆盖备份、查看、还原、筛选、空目录、错误密码和 GUI 产物检查，结果全部通过。"""
        ],
        13: [
            """七、AI 开发工具使用总结：
本项目开发过程中使用了 Codex 桌面环境中的 AI 编程助手辅助完成需求解析、代码实现、调试、测试和文档生成。

1. 工具介绍
Codex 是面向代码任务的 AI 开发助手，可读取本地工作区文件、执行构建和测试命令、根据错误信息定位问题，并在用户确认的工作区内生成或修改代码和文档。

2. 使用方法
（1）需求理解：根据课程 PDF 提取实验要求，确定基础功能和扩展功能范围。
（2）项目搭建：生成 C++17 项目结构，包括 `src`、`tests`、`docs`、`tools` 和 Makefile。
（3）编码实现：辅助实现 `.sdb` 归档格式、目录扫描、压缩、加密、还原、CLI 和 GUI。
（4）错误修复：根据 MinGW 8.1.0 对 `<filesystem>` 支持不完整的问题，调整为 Win32 文件系统适配层。
（5）测试验证：运行 `mingw32-make test`，根据失败信息修复 PowerShell 错误处理和构建依赖。
（6）文档生成：整理需求分析说明书、系统设计文档、软件测试报告、答辩 PPT 和本实验报告。

3. 作用效果评价
AI 工具显著提升了项目搭建、样板代码编写、错误定位和文档整理效率。尤其在编译器兼容性问题上，AI 能根据错误日志快速提出替代方案并完成代码迁移。需要注意的是，AI 生成内容仍需人工审核，例如加密算法安全级别、测试覆盖范围、报告中的小组姓名学号等信息必须由项目组最终确认和补充。"""
        ],
        14: [
            """八、实验结论：
本项目最终实现了一款基本可用的数据备份软件，满足实验基础要求，并完成多个扩展功能。

已实现功能：
（1）数据备份：递归保存目录树中的普通文件和空目录。
（2）数据还原：从 `.sdb` 归档恢复目录结构和文件内容。
（3）归档查看：命令行和 GUI 均可查看归档条目。
（4）元数据支持：保存并恢复 Windows 文件属性和最后修改时间。
（5）自定义备份：支持扩展名、名称、大小、修改时间筛选。
（6）打包解包：将所有条目写入单个归档文件。
（7）压缩解压：实现 RLE 压缩，压缩有效时保存压缩数据。
（8）加密解密：支持用户密码加密文件载荷，并在还原时校验密码和校验和。
（9）图形界面：实现 `sdb_gui.exe`，提供 Backup、Restore、Archive List 三个页签。

采用技术：
C++17、Win32 API、二进制文件读写、小端序序列化、FNV-1a 校验、RLE 压缩、SplitMix64 伪随机流异或加密、PowerShell 自动化测试、Office Open XML 文档生成。

实现效果：
命令行程序 `bin/sdb.exe` 和 GUI 程序 `bin/sdb_gui.exe` 均已构建成功；自动化测试 `mingw32-make test` 通过；答辩 PPT 和实验文档已生成。"""
        ],
        15: [
            """九、总结及心得体会：
本项目完整经历了从需求分析、系统设计、编码实现到测试验证的过程。项目初期首先根据实验要求确定基础功能必须包括目录备份和数据还原；随后选择实现打包、压缩、加密、元数据、自定义备份和 GUI 作为扩展功能。

开发过程中遇到的主要问题包括：
（1）MinGW g++ 8.1.0 对 `<filesystem>` 支持不完整，初版代码无法正常编译。解决办法是改用 Win32 API 实现目录遍历、路径处理、目录创建、文件属性和时间恢复。
（2）加密和压缩的处理顺序需要明确。最终采用“备份时先压缩再加密，还原时先解密再解压”的顺序，并通过校验和验证数据正确性。
（3）还原路径安全需要重点处理。最终在读取归档条目时拒绝绝对路径、空路径、`.` 和 `..`，避免恶意归档写出目标目录。
（4）GUI 与 CLI 不能各自实现一套备份逻辑，否则容易出现行为不一致。最终将核心功能集中在 `backup.hpp` / `backup.cpp`，两个入口只负责参数或控件输入。

通过本实验，项目组对文件系统编程、二进制格式设计、错误处理、自动化测试和工程文档编写有了更完整的认识。相比单个课程作业，本实验更强调完整生命周期和交付物一致性，也更接近真实软件项目开发流程。"""
        ],
        16: [
            """十、对本实验过程及方法、手段的改进建议：
（1）建议在实验前期提供若干归档格式、路径安全、文件元数据处理的参考案例，帮助学生更快理解备份软件的关键难点。
（2）建议明确各扩展功能的验收口径，例如加密算法是否必须使用专业密码库、压缩算法是否允许自定义简单算法、GUI 是否限定框架。
（3）建议在课程中加入一次中期检查，重点检查需求分析和系统设计，避免后期只关注代码而忽略文档一致性。
（4）建议提供标准测试样例，包括多级目录、空目录、大文件、错误密码、路径逃逸归档等，便于各小组自测。
（5）项目组后续可继续改进本软件：增加进度条、任务取消、日志文件、增量备份、定时备份、实时备份和网络备份。"""
        ],
        17: ["报告评分：                                 指导教师签字："],
    }


def update_core_props(data: bytes) -> bytes:
    try:
        root = ET.fromstring(data)
    except ET.ParseError:
        return data
    title = root.find(qn(DC_NS, "title"))
    if title is None:
        title = ET.SubElement(root, qn(DC_NS, "title"))
    title.text = "文件备份软件实验报告"
    return ET.tostring(root, encoding="utf-8", xml_declaration=True)


def build_report() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(TEMPLATE, "r") as zin:
        document_root = ET.fromstring(zin.read("word/document.xml"))
        body = document_root.find(w("body"))
        if body is None:
            raise RuntimeError("template document has no body")
        table = body.find(w("tbl"))
        if table is None:
            raise RuntimeError("template document has no table")

        rows = table.findall(w("tr"))
        replacements = table_texts()
        for row_index, texts in replacements.items():
            if row_index >= len(rows):
                continue
            row = rows[row_index]
            remove_fixed_row_height(row)
            cells = cells_of(row)
            for cell_index, text in enumerate(texts):
                if cell_index < len(cells):
                    set_cell_text(cells[cell_index], text)

        document_xml = ET.tostring(document_root, encoding="utf-8", xml_declaration=True)

        with zipfile.ZipFile(OUTPUT, "w", zipfile.ZIP_DEFLATED) as zout:
            for item in zin.infolist():
                data = zin.read(item.filename)
                if item.filename == "word/document.xml":
                    data = document_xml
                elif item.filename == "docProps/core.xml":
                    data = update_core_props(data)
                zout.writestr(item, data)


if __name__ == "__main__":
    build_report()
