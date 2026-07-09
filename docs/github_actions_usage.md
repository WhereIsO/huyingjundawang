# GitHub Actions 与 Release 使用说明

本文面向第一次使用 GitHub Actions 的组员，说明如何自动构建、下载可执行程序、发布 Release。

## 1. Actions 是什么

GitHub Actions 是 GitHub 的云端自动化任务。当前仓库新增了 `Windows Build and Release` workflow，它会在 Windows 云主机上完成：

1. 拉取源码。
2. 安装 Qt。
3. 使用 Visual Studio 2022 + CMake 构建 Release 版本。
4. 运行 CTest。
5. 使用 `windeployqt` 打包可运行程序。
6. 上传 zip 构建产物。
7. 当推送 `v*` 标签时，自动创建或更新 GitHub Release。

workflow 文件位置：

```text
.github/workflows/windows-build-release.yml
```

## 2. 普通提交和 PR 会发生什么

当有人 push 分支或创建 PR 时，GitHub 会自动运行构建和测试。

查看方法：

1. 打开 GitHub 仓库页面。
2. 点击顶部 `Actions`。
3. 左侧选择 `Windows Build and Release`。
4. 点击最新一次运行记录。
5. 如果所有步骤都是绿色，说明云端构建和测试通过。

运行结束后，在页面底部可以看到 `Artifacts`，下载 `BackupTool-windows-x64` 即可得到 zip 包。这个 zip 是临时构建产物，不是正式 Release。

## 3. 手动运行一次 Actions

适合在答辩前确认最新代码能否构建。

1. 打开仓库 `Actions` 页面。
2. 选择 `Windows Build and Release`。
3. 点击 `Run workflow`。
4. 选择要构建的分支，例如 `main`。
5. 点击绿色 `Run workflow` 按钮。
6. 等待运行结束，下载页面底部的 artifact。

## 4. 发布正式 Release

正式分发给老师或同学时，建议使用 GitHub Release，而不是把 exe/dll 直接提交进仓库。

推荐流程：

```powershell
git checkout main
git pull
git tag v0.4.0
git push origin v0.4.0
```

推送 `v0.4.0` 这种以 `v` 开头的标签后，Actions 会自动：

1. 构建 Release 版本。
2. 运行测试。
3. 打包 `BackupTool-windows-x64.zip`。
4. 在 GitHub 的 `Releases` 页面创建 `BackupTool v0.4.0`。
5. 上传 zip 作为 Release 附件。

如果同一个 tag 的 Release 已经存在，workflow 会覆盖上传 zip 附件。

## 5. 本地如何生成同样的包

先完成本地构建，然后执行：

```powershell
$env:QT_PREFIX = "<Qt安装目录>"
.\scripts\package_windows.ps1 -BuildDir build -Config Release
```

脚本会：

1. 找到 `build/Release/BackupTool.exe` 或 `build/BackupTool.exe`。
2. 复制到 `build/package/BackupTool-windows-x64`。
3. 调用 `windeployqt` 收集 Qt 运行时。
4. 生成 `build/BackupTool-windows-x64.zip`。

## 6. 常见问题

### artifact 和 release 有什么区别

artifact 是每次 Actions 运行产生的临时构建产物，适合测试下载。Release 是正式版本发布，适合交付或发给同学。

### 仓库为什么不直接提交 exe

exe、dll、zip 属于构建产物，会让仓库变大，也不利于代码审查。源码进入 Git，二进制进入 Actions artifact 或 GitHub Release。

### 没有本地编译环境怎么办

不需要在本地编译。让 GitHub Actions 云端构建，然后下载 artifact 或 Release zip。

### Actions 失败怎么办

先点开失败步骤查看红色日志。常见原因包括 Qt 下载失败、测试失败、CMake 配置失败。修复代码后重新 push，Actions 会自动再跑。
