# 项目演示视频脚本

建议录屏时长控制在 3 分钟以内。

## 1. 项目简介

本项目是一个 C++17 数据备份软件，支持目录备份、还原、归档查看，并实现了打包、压缩、加密、元数据、自定义筛选和图形界面。

## 2. 构建演示

```powershell
mingw32-make
```

说明生成可执行程序：

```text
bin/sdb.exe
bin/sdb_gui.exe
```

## 3. 自动化测试演示

```powershell
mingw32-make test
```

展示输出末尾：

```text
All tests passed.
```

## 4. 图形界面演示

```powershell
.\bin\sdb_gui.exe
```

展示 Backup、Restore、Archive List 三个页签，说明可通过浏览按钮选择目录和归档文件，并可设置压缩、密码、筛选和覆盖选项。

## 5. 命令行功能演示

创建备份：

```powershell
.\bin\sdb.exe backup .\src .\src_backup.sdb --compress --password 123456 --exclude-ext .tmp
```

查看归档：

```powershell
.\bin\sdb.exe list .\src_backup.sdb
```

还原归档：

```powershell
.\bin\sdb.exe restore .\src_backup.sdb .\restore_demo --password 123456
```

演示结束后可删除临时文件：

```powershell
Remove-Item -Recurse -Force .\src_backup.sdb, .\restore_demo
```
