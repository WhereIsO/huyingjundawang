# backup_core 模块说明

`src/core` 是数据备份软件的纯 C++ 后端核心，不依赖 Qt。GUI 通过 `RealBackend` 将 Qt 请求转换成 core 的 `BackupTask` / `RestoreTask` 调用。

## 模块职责

- `types.*`：公共数据结构、错误码、进度、日志等级。
- `encoding.*`：Windows 宽字符路径与 UTF-8 字符串转换，支持中文路径。
- `checksum.*`：CRC32 与 Windows CNG SHA-256。
- `binary_io.*`：二进制序列化辅助。
- `filter.*`：路径、类型、名称、时间、尺寸、用户 6 类筛选。
- `huffman.*`：自研哈夫曼压缩与解压。
- `crypto.*`：Windows CNG AES-256-GCM 与 PBKDF2-SHA256。
- `metadata.*`：Windows 文件属性、时间戳、Owner SID、ACL 摘要采集和尽力恢复。
- `scanner.*`：目录树扫描，识别普通文件、目录、空目录、符号链接、硬链接、Junction/ReparsePoint。
- `archive.*`：`.pbackup` 备份包读写与完整性校验。
- `backup_task.*`：备份/还原主流程调度。

## 当前实现范围

已实现：

- 目录树备份到单个 `.pbackup` 文件。
- 从 `.pbackup` 还原到指定目录。
- 文件类型识别与恢复：普通文件、目录、空目录、符号链接、硬链接、Junction；未知 ReparsePoint 记录并按目录尽力恢复。
- 元数据采集：大小、Windows 属性、创建/访问/修改时间、Owner SID、ACL 摘要。
- 6 类筛选：路径、类型、名字、时间、尺寸、用户。
- 哈夫曼压缩/解压。
- AES-256-GCM 加密/解密，PBKDF2-SHA256 派生密钥。
- CRC32 / SHA-256 完整性校验。
- 取消标志和进度回调。

限制与说明：

- Windows 符号链接创建可能需要开发者模式或管理员权限；失败时记录警告。
- 未知 ReparsePoint 当前按目录尽力恢复，并记录警告。
- ACL 当前保存摘要用于证明采集，完整 ACL 还原按权限尽力而为。
- 加密使用 Windows CNG，不引入 OpenSSL 部署依赖，符合“禁止手搓密码学算法”的要求。

## 测试

`backup_tests` 使用 GoogleTest，覆盖哈夫曼、加密、筛选、归档、备份还原、扫描、元数据、路径穿越防护和进度回调。连同真实后端和 GUI 信号 smoke test，CTest 总计 44 个测试项。

推荐构建命令：

```powershell
$env:QT_PREFIX = "<Qt安装目录>"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$env:QT_PREFIX" -DCMAKE_CXX_COMPILER=cl
cmake --build build
ctest --test-dir build --output-on-failure
```

注意：如果 Qt AUTOMOC 在中文路径下出现编码问题，可先将项目复制到短英文路径后构建。
