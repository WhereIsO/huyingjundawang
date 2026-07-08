# Simple Data Backup

项目使用 C++17 编写，提供命令行和图形界面两种使用方式，支持备份、还原和归档查看。

## 已实现功能

- 目录树备份：递归保存目录、普通文件和空目录。
- 数据还原：从归档恢复目录结构和文件内容。
- 元数据支持：保存并尽量恢复修改时间和文件权限。
- 自定义备份：支持按扩展名、文件名、大小、修改时间筛选。
- 打包解包：将所有备份内容写入单个 `.sdb` 归档文件。
- 压缩解压：内置 RLE 压缩，只有压缩后更小时才保存压缩数据。
- 加密解密：可用 `--password` 对文件载荷加密，恢复时校验密码和内容校验和。
- 图形界面：提供备份、还原、归档查看三个页签，支持目录/文件选择和状态提示。

## 构建

当前环境可使用 MinGW：

```powershell
mingw32-make
```

生成程序：

```text
bin/sdb.exe
bin/sdb_gui.exe
```

## 图形界面使用

运行：

```powershell
.\bin\sdb_gui.exe
```

界面包含三个页签：

- Backup：选择源目录、归档文件、压缩/密码和筛选条件后执行备份。
- Restore：选择归档文件和目标目录后执行还原，可选择覆盖已有文件。
- Archive List：选择 `.sdb` 归档后查看内部目录和文件条目。

## 命令行使用示例

备份目录：

```powershell
.\bin\sdb.exe backup C:\data C:\backup\data.sdb
```

启用压缩和密码：

```powershell
.\bin\sdb.exe backup C:\data C:\backup\data.sdb --compress --password 123456
```

排除临时文件：

```powershell
.\bin\sdb.exe backup C:\data C:\backup\data.sdb --exclude-ext .tmp,.log
```

还原：

```powershell
.\bin\sdb.exe restore C:\backup\data.sdb C:\restore --password 123456
```

查看归档内容：

```powershell
.\bin\sdb.exe list C:\backup\data.sdb
```

## 测试

```powershell
mingw32-make test
```

测试脚本会创建临时源目录，执行加密压缩备份、归档查看、还原、过滤规则和错误密码校验。
