# `.pbackup` 备份包格式说明

本文档描述 `backup_core` 当前实现的 `.pbackup` 单文件归档格式。格式面向 Windows 平台，所有多字节整数均使用小端序。

## 1. 总体结构

```text
FileHeader    固定 64 字节
GlobalConfig  TLV 列表
IndexEntry[]  TLV 列表
Payload       文件数据区，可压缩、可整体加密
Trailer       PayloadCRC32 + PackageSHA256
```

## 2. FileHeader

| 偏移 | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 8 | Magic | ASCII `PBACKUP\0` |
| 8 | 2 | VersionMajor | 当前为 `1` |
| 10 | 2 | VersionMinor | 当前为 `0` |
| 12 | 1 | Platform | `0x01` 表示 Windows |
| 13 | 1 | Flags | bit0=哈夫曼压缩，bit1=AES-GCM 加密 |
| 14 | 4 | IndexCount | 索引条目数量 |
| 18 | 4 | IndexOffset | IndexEntry 区起始偏移 |
| 22 | 4 | PayloadOffset | Payload 区起始偏移 |
| 26 | 8 | CreatedUnixNs | 创建时间，Unix 纳秒 |
| 34 | 16 | Reserved | 保留，写 0 |
| 50 | 8 | Salt | 加密时的 PBKDF2 盐；未加密写 0 |
| 58 | 2 | KdfIters16 | 兼容字段；真实迭代次数见 GlobalConfig |
| 60 | 4 | HeaderCRC32 | 前 60 字节 CRC32 |

说明：课程模板给出的 header 中 `KdfIters` 为 2 字节，但目标迭代次数为 200000，超过 16 位上限。因此实现将真实 `kdf_iters` 写入 GlobalConfig 的 `0x0005` TLV 中，Header 内保留兼容值。

## 3. GlobalConfig

GlobalConfig 是连续 TLV：

```text
Tag    uint16
Length uint32
Value  Length 字节
```

当前写入：

| Tag | 名称 | 内容 |
|---:|---|---|
| `0x0001` | ToolName | UTF-8 字符串 `BackupTool/1.0` |
| `0x0002` | CompressionAlgo | `uint32`，`0` 表示 Huffman，`0xFFFFFFFF` 表示未压缩 |
| `0x0003` | EncryptionAlgo | `uint32`，`0` 表示无加密，`1` 表示 AES-256-GCM |
| `0x0004` | SourceRoot | UTF-8 源目录字符串 |
| `0x0005` | KdfIters | `uint32`，PBKDF2-SHA256 迭代次数，默认 200000 |

## 4. IndexEntry

每个索引条目也是 TLV，Tag 固定为 `0x0010`。Value 内部字段顺序如下：

```text
RelPath     UTF-8 NUL 结尾，相对源根目录
EntryType   uint8
FileSize    uint64，原始文件大小
MTimeNs     int64
ATimeNs     int64
CTimeNs     int64
Attributes  uint32，Windows GetFileAttributes 结果
OwnerSid    UTF-8 NUL 结尾
AclDigest   32 字节，安全描述符 SHA-256 摘要，未采集时全 0
TargetPath  UTF-8 NUL 结尾，链接类条目使用
DataOffset  uint64，指向明文 Payload 中该文件数据的偏移
DataLen     uint64，明文 Payload 中该文件数据长度
EntryCRC32  uint32，前述 Value 字节的 CRC32
```

EntryType 取值：

| 值 | 类型 |
|---:|---|
| 0 | 普通文件 |
| 1 | 普通目录 |
| 2 | 空目录 |
| 3 | 符号链接 |
| 4 | 硬链接 |
| 5 | Junction |
| 6 | ReparsePoint |

## 5. Payload

Payload 存储所有普通文件的数据。目录、空目录、符号链接、硬链接、Junction、ReparsePoint 不写入文件数据。

未加密时：

```text
Payload = FileChunk[0] + FileChunk[1] + ...
```

启用压缩时，每个普通文件独立执行哈夫曼压缩：

```text
FileChunk = HUF1 + OriginalSize + FrequencyTable[256] + BitStream
```

启用加密时，对整个明文 Payload 执行 AES-256-GCM：

```text
StoredPayload = IV(12B) + Tag(16B) + CipherText
```

密钥派生：

- KDF：PBKDF2-SHA256
- Key：32 字节
- Salt：FileHeader 中的 8 字节随机盐
- 默认迭代次数：200000
- 实现 API：Windows CNG (`bcrypt`)

读取时先校验包完整性，再解密 Payload，最后按索引条目的 `DataOffset/DataLen` 取出每个文件块并解压。

## 6. Trailer

Trailer 位于文件末尾：

| 长度 | 字段 | 说明 |
|---:|---|---|
| 4 | PayloadCRC32 | 对实际存储的 Payload 计算 CRC32；若加密则对密文区计算 |
| 32 | PackageSHA256 | 对除本字段外的整个包计算 SHA-256 |

读取时会依次校验：

1. FileHeader Magic 与 HeaderCRC32。
2. PackageSHA256。
3. PayloadCRC32。
4. 每个 IndexEntry 的 EntryCRC32。
5. AES-GCM 认证标签。
6. 哈夫曼数据头与码流完整性。

任一校验失败均抛出 `BackupError`，错误码为 `PkgCorrupted` 或 `InvalidPassword`。
