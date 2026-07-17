[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$script:Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$script:Core = Join-Path $script:Root "backend\pbackup_core.exe"
$script:Work = Join-Path $PSScriptRoot "work"
$script:ResultsDir = Join-Path $PSScriptRoot "results"
$script:FixtureRoot = Join-Path $script:Work "fixtures"
$script:FolderSource = Join-Path $script:FixtureRoot "source-folder"
$script:SingleSource = Join-Path $script:FixtureRoot "standalone.txt"
$script:Definitions = [System.Collections.Generic.List[object]]::new()
$script:Results = [System.Collections.Generic.List[object]]::new()
$script:BackendEvents = [System.Collections.Generic.List[string]]::new()

function Remove-SafeTree {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $resolvedRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path
    $resolvedTarget = (Resolve-Path -LiteralPath $Path).Path
    if (-not $resolvedTarget.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "拒绝删除 tests 目录外的路径：$resolvedTarget"
    }
    Remove-Item -LiteralPath $resolvedTarget -Recurse -Force
}

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Assert-Equal {
    param($Expected, $Actual, [string]$Message)
    if ($Expected -ne $Actual) {
        throw "$Message；期望=$Expected，实际=$Actual"
    }
}

function ConvertTo-ProjectRelativeArgument {
    param([Parameter(Mandatory)][string]$Value)
    $rootPrefix = $script:Root.TrimEnd('\') + '\'
    if ($Value.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $Value.Substring($rootPrefix.Length).Replace('\', '/')
    }
    return $Value
}

function Invoke-Core {
    param(
        [Parameter(Mandatory)][string[]]$Arguments,
        [AllowEmptyString()][string]$Password = ""
    )
    $hadPassword = Test-Path Env:PBACKUP_PASSWORD
    $previousPassword = $env:PBACKUP_PASSWORD
    try {
        if ([string]::IsNullOrEmpty($Password)) {
            Remove-Item Env:PBACKUP_PASSWORD -ErrorAction SilentlyContinue
        } else {
            $env:PBACKUP_PASSWORD = $Password
        }
        $commandArguments = @($Arguments | ForEach-Object { ConvertTo-ProjectRelativeArgument $_ })
        Push-Location -LiteralPath $script:Root
        try {
            $lines = @(& $script:Core @commandArguments 2>&1 | ForEach-Object { $_.ToString() })
            $exitCode = $LASTEXITCODE
        } finally {
            Pop-Location
        }
    } finally {
        if ($hadPassword) { $env:PBACKUP_PASSWORD = $previousPassword }
        else { Remove-Item Env:PBACKUP_PASSWORD -ErrorAction SilentlyContinue }
    }
    $script:BackendEvents.Add("===== pbackup_core.exe $($commandArguments -join ' ') =====")
    foreach ($line in $lines) { $script:BackendEvents.Add($line) }
    $script:BackendEvents.Add("EXIT_CODE=$exitCode")
    return [pscustomobject]@{
        ExitCode = $exitCode
        Lines = $lines
        Text = ($lines -join "`n")
    }
}

function Get-CoreResultData {
    param([Parameter(Mandatory)]$Invocation)
    foreach ($line in $Invocation.Lines) {
        try {
            $event = $line | ConvertFrom-Json -ErrorAction Stop
            if ($event.type -eq "result") { return $event.data }
        } catch {
            continue
        }
    }
    throw "后端输出中没有 result JSON 事件。"
}

function Get-RelativeFileMap {
    param([Parameter(Mandatory)][string]$RootPath)
    $map = @{}
    foreach ($file in Get-ChildItem -LiteralPath $RootPath -File -Recurse) {
        $rootFull = (Resolve-Path -LiteralPath $RootPath).Path.TrimEnd('\')
        $relative = $file.FullName.Substring($rootFull.Length).TrimStart('\').Replace('\', '/')
        $map[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
    return $map
}

function Assert-FolderEqual {
    param([string]$ExpectedRoot, [string]$ActualRoot)
    $expected = Get-RelativeFileMap $ExpectedRoot
    $actual = Get-RelativeFileMap $ActualRoot
    Assert-Equal $expected.Count $actual.Count "恢复文件数量不一致"
    foreach ($key in $expected.Keys) {
        Assert-True ($actual.ContainsKey($key)) "恢复结果缺少文件：$key"
        Assert-Equal $expected[$key] $actual[$key] "文件哈希不一致：$key"
    }
}

function Add-Test {
    param(
        [string]$Id,
        [string]$Name,
        [string]$Category,
        [string]$Expected,
        [scriptblock]$Action
    )
    $script:Definitions.Add([pscustomobject]@{
        编号 = $Id
        名称 = $Name
        分类 = $Category
        前置条件 = "C++ 后端已构建，测试夹具由脚本自动生成"
        操作 = $Name
        预期结果 = $Expected
    })

    $watch = [Diagnostics.Stopwatch]::StartNew()
    $status = "通过"
    $detail = ""
    try {
        $detail = [string](& $Action)
        if ([string]::IsNullOrWhiteSpace($detail)) { $detail = "断言全部通过" }
    } catch {
        $status = "失败"
        $detail = $_.Exception.Message
    }
    $watch.Stop()
    $script:Results.Add([pscustomobject]@{
        编号 = $Id
        名称 = $Name
        分类 = $Category
        状态 = $status
        耗时毫秒 = $watch.ElapsedMilliseconds
        实际结果 = $detail
        执行时间 = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
    })
    $color = if ($status -eq "通过") { "Green" } else { "Red" }
    Write-Host ("[{0}] {1} {2} ({3} ms)" -f $status, $Id, $Name, $watch.ElapsedMilliseconds) -ForegroundColor $color
}

function New-CasePaths {
    param([string]$Id)
    $base = Join-Path $script:Work $Id
    [IO.Directory]::CreateDirectory($base) | Out-Null
    return [pscustomobject]@{
        Base = $base
        Package = Join-Path $base "data.pbackup"
        Restore = Join-Path $base "restore"
    }
}

function Test-SingleMatrixCase {
    param([string]$Id, [string]$Compression, [string]$Encryption)
    $paths = New-CasePaths $Id
    $password = if ($Encryption -eq "none") { "" } else { "Matrix-Pass-2026!" }
    $backup = Invoke-Core @("backup", "--source", $script:SingleSource, "--output", $paths.Package,
        "--compression", $Compression, "--encryption", $Encryption, "--kdf", "100000") $password
    Assert-Equal 0 $backup.ExitCode "单文件备份失败"
    $restore = Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1") $password
    Assert-Equal 0 $restore.ExitCode "单文件恢复失败"
    $restored = Join-Path $paths.Restore "standalone.txt"
    Assert-True (Test-Path -LiteralPath $restored -PathType Leaf) "恢复后缺少 standalone.txt"
    Assert-Equal (Get-FileHash $script:SingleSource -Algorithm SHA256).Hash (Get-FileHash $restored -Algorithm SHA256).Hash "单文件恢复哈希不一致"
    $data = Get-CoreResultData $restore
    Assert-Equal $Compression $data.compression "恢复结果压缩标识不一致"
    Assert-Equal $Encryption $data.encryption "恢复结果加密标识不一致"
    return "$Compression + $Encryption 备份、自动校验恢复及 SHA-256 比对通过"
}

function Test-FolderCase {
    param([string]$Id, [string]$Compression, [string]$Encryption)
    $paths = New-CasePaths $Id
    $password = if ($Encryption -eq "none") { "" } else { "Folder-Pass-2026!" }
    $backup = Invoke-Core @("backup", "--source", $script:FolderSource, "--output", $paths.Package,
        "--compression", $Compression, "--encryption", $Encryption, "--kdf", "100000") $password
    Assert-Equal 0 $backup.ExitCode "文件夹备份失败"
    $restore = Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1") $password
    Assert-Equal 0 $restore.ExitCode "文件夹恢复失败"
    Assert-FolderEqual $script:FolderSource $paths.Restore
    $files = (Get-ChildItem -LiteralPath $paths.Restore -File -Recurse).Count
    return "$Compression + $Encryption 文件夹恢复完成，$files 个文件哈希一致"
}

function New-EncryptedPackage {
    param([string]$Id, [string]$Encryption, [string]$Password = "Security-Pass-2026!")
    $paths = New-CasePaths $Id
    $backup = Invoke-Core @("backup", "--source", $script:SingleSource, "--output", $paths.Package,
        "--compression", "stored", "--encryption", $Encryption, "--kdf", "100000") $Password
    Assert-Equal 0 $backup.ExitCode "准备加密测试包失败"
    return [pscustomobject]@{ Paths = $paths; Password = $Password }
}

function Corrupt-Package {
    param([string]$Source, [string]$Destination)
    [IO.File]::Copy($Source, $Destination, $true)
    $bytes = [IO.File]::ReadAllBytes($Destination)
    Assert-True ($bytes.Length -gt 116) "测试包长度不足，无法执行篡改测试"
    $bytes[$bytes.Length - 1] = $bytes[$bytes.Length - 1] -bxor 0x5A
    [IO.File]::WriteAllBytes($Destination, $bytes)
}

function Test-TamperRejected {
    param([string]$Id, [string]$Encryption)
    $password = if ($Encryption -eq "none") { "" } else { "Tamper-Pass-2026!" }
    $prepared = New-EncryptedPackage $Id $Encryption $password
    $corrupt = Join-Path $prepared.Paths.Base "corrupt.pbackup"
    Corrupt-Package $prepared.Paths.Package $corrupt
    $destination = Join-Path $prepared.Paths.Base "corrupt-restore"
    $restore = Invoke-Core @("restore", "--package", $corrupt, "--destination", $destination, "--overwrite", "1") $password
    Assert-True ($restore.ExitCode -ne 0) "被篡改的备份包被错误接受"
    Assert-True ($restore.Text -match "损坏|校验失败|解密失败") "错误信息未说明损坏或校验失败"
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $destination "standalone.txt"))) "损坏包写出了正式恢复文件"
    return "$Encryption 负载末字节篡改后恢复被拒绝"
}

function Invoke-GuiProbe {
    $pythonCode = @"
import json, os
os.environ['QT_QPA_PLATFORM'] = 'offscreen'
from app.qt_compat import QtCore, QtWidgets
QtCore.QSettings.setDefaultFormat(QtCore.QSettings.IniFormat)
QtCore.QSettings.setPath(QtCore.QSettings.IniFormat, QtCore.QSettings.UserScope, os.path.abspath('tests/work/qsettings'))
from app.gui import MainWindow
app = QtWidgets.QApplication.instance() or QtWidgets.QApplication([])
w = MainWindow()
data = {
    'page_count': w.stack.count(),
    'labels': [b.text() for b in w.nav_buttons],
    'has_verify_page': hasattr(w, 'verify_page'),
    'has_chacha': w.encryption_combo.findData('chacha20_poly1305') >= 0,
    'has_background_controls': hasattr(w, 'change_background_button') and hasattr(w, 'reset_background_button')
}
print(json.dumps(data, ensure_ascii=True))
w.close()
"@
    $output = @(& python -c $pythonCode 2>&1 | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) { throw "GUI 探针执行失败：$($output -join ' ')" }
    return ($output[-1] | ConvertFrom-Json)
}

if (-not (Test-Path -LiteralPath $script:Core -PathType Leaf)) {
    throw "缺少 C++ 后端：$script:Core"
}

Remove-SafeTree $script:Work
[IO.Directory]::CreateDirectory($script:Work) | Out-Null
[IO.Directory]::CreateDirectory($script:ResultsDir) | Out-Null
[IO.Directory]::CreateDirectory($script:FolderSource) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $script:FolderSource "nested")) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $script:FolderSource "中文目录")) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $script:FixtureRoot "中文单文件")) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($script:SingleSource, "standalone file`nline 2`n中文内容`n", $utf8)
[IO.File]::WriteAllText((Join-Path $script:FolderSource "alpha.txt"), "alpha`n", $utf8)
[IO.File]::WriteAllText((Join-Path $script:FolderSource "nested\beta.txt"), "beta 中文`n", $utf8)
[IO.File]::WriteAllText((Join-Path $script:FolderSource "nested\skip.tmp"), "temporary`n", $utf8)
[IO.File]::WriteAllText((Join-Path $script:FolderSource "report_01.docx"), "mock docx payload`n", $utf8)
[IO.File]::WriteAllBytes((Join-Path $script:FolderSource "empty.bin"), [byte[]]@())
[IO.File]::WriteAllText((Join-Path $script:FolderSource "中文目录\数据.txt"), "中文路径测试`n", $utf8)
$binary = [byte[]]::new(65536)
for ($i = 0; $i -lt $binary.Length; $i++) { $binary[$i] = [byte]($i % 251) }
[IO.File]::WriteAllBytes((Join-Path $script:FolderSource "binary.dat"), $binary)
$unicodeSingle = Join-Path $script:FixtureRoot "中文单文件\测试文件.txt"
[IO.File]::WriteAllText($unicodeSingle, "单文件中文路径`n", $utf8)

$capInvocation = Invoke-Core @("capabilities")
$capData = if ($capInvocation.ExitCode -eq 0) { Get-CoreResultData $capInvocation } else { $null }
$guiData = Invoke-GuiProbe

Add-Test "TC-001" "后端能力查询命令" "接口" "capabilities 返回成功" {
    Assert-Equal 0 $capInvocation.ExitCode "capabilities 执行失败"
    "退出码为 0"
}
Add-Test "TC-002" "备份格式标识" "格式" "返回 PBACKUP-CPP3" {
    Assert-Equal "PBACKUP-CPP3" $capData.format "格式标识错误"
    "格式为 PBACKUP-CPP3"
}
Add-Test "TC-003" "系统支持的压缩能力" "压缩" "返回 stored，且在新版 Windows 上返回全部压缩算法" {
    $compressionCount = @($capData.compression.psobject.Properties).Count
    Assert-True ($compressionCount -eq 1 -or $compressionCount -eq 5) "压缩算法数量错误"
    Assert-True ($null -ne $capData.compression.stored) "缺少 stored 无压缩模式"
    if ($compressionCount -eq 5) {
        "Stored、MSZIP、XPRESS、XPRESS Huffman、LZMS 均可用"
    } else {
        "系统未提供 Compression API，仅使用 stored 无压缩模式"
    }
}
Add-Test "TC-004" "三种加密状态" "加密" "返回不加密及两种认证加密" {
    Assert-Equal 3 @($capData.encryption.psobject.Properties).Count "加密选项数量错误"
    "none、AES-GCM、ChaCha20-Poly1305 均可用"
}
Add-Test "TC-005" "AES-256-GCM 能力" "加密" "包含 aes256_gcm" {
    Assert-True ($null -ne $capData.encryption.aes256_gcm) "缺少 AES-GCM"
    $capData.encryption.aes256_gcm
}
Add-Test "TC-006" "ChaCha20-Poly1305 能力" "加密" "包含 chacha20_poly1305" {
    Assert-True ($null -ne $capData.encryption.chacha20_poly1305) "缺少 ChaCha20-Poly1305"
    $capData.encryption.chacha20_poly1305
}
Add-Test "TC-007" "GUI 页面数量" "GUI" "主窗口仅包含三个页面" {
    Assert-Equal 3 $guiData.page_count "GUI 页面数量错误"
    "页面数量为 3"
}
Add-Test "TC-008" "GUI 删除独立校验页" "GUI" "导航仅为创建、恢复、日志且无 verify_page" {
    Assert-True (-not $guiData.has_verify_page) "仍存在独立校验页面属性"
    Assert-Equal "创建备份|恢复数据|运行日志" ($guiData.labels -join '|') "导航名称错误"
    "独立校验页不存在"
}
Add-Test "TC-009" "GUI 显示 ChaCha20 选项" "GUI" "加密下拉框包含 ChaCha20-Poly1305" {
    Assert-True $guiData.has_chacha "GUI 未加载 ChaCha20-Poly1305"
    "ChaCha20-Poly1305 已显示"
}
Add-Test "TC-010" "GUI 自定义背景控件" "GUI" "包含选择背景和恢复默认按钮" {
    Assert-True $guiData.has_background_controls "缺少背景设置控件"
    "两个背景控制按钮均存在"
}

$compressions = @($capData.compression.psobject.Properties.Name)
$preferredCompression = if ($compressions -contains "mszip") { "mszip" } else { "stored" }
$encryptions = @("none", "aes256_gcm", "chacha20_poly1305")
$caseNumber = 11
foreach ($compression in $compressions) {
    foreach ($encryption in $encryptions) {
        $id = "TC-{0:D3}" -f $caseNumber
        $name = "单文件组合：$compression + $encryption"
        Add-Test $id $name "组合回归" "备份、恢复和 SHA-256 比对全部通过" {
            Test-SingleMatrixCase $id $compression $encryption
        }
        $caseNumber++
    }
}

foreach ($compression in $compressions) {
    $id = "TC-{0:D3}" -f $caseNumber
    Add-Test $id "文件夹压缩恢复：$compression" "文件夹" "完整目录树恢复且文件哈希一致" {
        Test-FolderCase $id $compression "none"
    }
    $caseNumber++
}

Add-Test "TC-031" "AES-GCM 文件夹恢复" "加密" "加密文件夹自动校验恢复成功" {
    Test-FolderCase "TC-031" $preferredCompression "aes256_gcm"
}
Add-Test "TC-032" "ChaCha20 文件夹恢复" "加密" "加密文件夹自动校验恢复成功" {
    Test-FolderCase "TC-032" $preferredCompression "chacha20_poly1305"
}
Add-Test "TC-033" "AES-GCM 错误密码" "安全" "错误密码被拒绝且不恢复正式文件" {
    $prepared = New-EncryptedPackage "TC-033" "aes256_gcm"
    $result = Invoke-Core @("restore", "--package", $prepared.Paths.Package, "--destination", $prepared.Paths.Restore, "--overwrite", "1") "wrong-password"
    Assert-True ($result.ExitCode -ne 0) "错误密码被接受"
    Assert-True ($result.Text -match "密码错误|损坏") "错误提示不正确"
    "AES-GCM 错误密码退出码=$($result.ExitCode)"
}
Add-Test "TC-034" "ChaCha20 错误密码" "安全" "错误密码被拒绝且不恢复正式文件" {
    $prepared = New-EncryptedPackage "TC-034" "chacha20_poly1305"
    $result = Invoke-Core @("restore", "--package", $prepared.Paths.Package, "--destination", $prepared.Paths.Restore, "--overwrite", "1") "wrong-password"
    Assert-True ($result.ExitCode -ne 0) "错误密码被接受"
    Assert-True ($result.Text -match "密码错误|损坏") "错误提示不正确"
    "ChaCha20-Poly1305 错误密码退出码=$($result.ExitCode)"
}
Add-Test "TC-035" "AES-GCM 损坏包拦截" "恢复自动校验" "修改密文后恢复认证失败" {
    Test-TamperRejected "TC-035" "aes256_gcm"
}
Add-Test "TC-036" "ChaCha20 损坏包拦截" "恢复自动校验" "修改密文后恢复认证失败" {
    Test-TamperRejected "TC-036" "chacha20_poly1305"
}
Add-Test "TC-037" "未加密包 SHA-256 损坏拦截" "恢复自动校验" "修改负载后归档 SHA-256 校验失败" {
    Test-TamperRejected "TC-037" "none"
}
Add-Test "TC-038" "禁止覆盖同名文件" "恢复" "overwrite=0 时拒绝第二次恢复" {
    $paths = New-CasePaths "TC-038"
    $backup = Invoke-Core @("backup", "--source", $script:SingleSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "none")
    Assert-Equal 0 $backup.ExitCode "准备备份失败"
    Assert-Equal 0 (Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1")).ExitCode "首次恢复失败"
    $second = Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "0")
    Assert-True ($second.ExitCode -ne 0) "未启用覆盖时仍覆盖成功"
    "第二次恢复被拒绝"
}
Add-Test "TC-039" "允许覆盖同名文件" "恢复" "overwrite=1 时可再次恢复" {
    $paths = New-CasePaths "TC-039"
    Assert-Equal 0 (Invoke-Core @("backup", "--source", $script:SingleSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "none")).ExitCode "准备备份失败"
    Assert-Equal 0 (Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1")).ExitCode "首次恢复失败"
    $second = Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1")
    Assert-Equal 0 $second.ExitCode "允许覆盖时恢复失败"
    "覆盖恢复成功"
}
Add-Test "TC-040" "包含路径关键词筛选" "筛选" "仅恢复 nested 路径下的两个文件" {
    $paths = New-CasePaths "TC-040"
    Assert-Equal 0 (Invoke-Core @("backup", "--source", $script:FolderSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "none", "--include", "nested")).ExitCode "筛选备份失败"
    Assert-Equal 0 (Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1")).ExitCode "筛选恢复失败"
    $files = Get-ChildItem -LiteralPath $paths.Restore -File -Recurse
    Assert-Equal 2 $files.Count "include 筛选文件数错误"
    Assert-True (Test-Path (Join-Path $paths.Restore "nested\beta.txt")) "缺少 beta.txt"
    "include=nested 恢复 2 个文件"
}
Add-Test "TC-041" "排除路径关键词筛选" "筛选" "skip.tmp 不进入备份" {
    $paths = New-CasePaths "TC-041"
    Assert-Equal 0 (Invoke-Core @("backup", "--source", $script:FolderSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "none", "--exclude", "skip")).ExitCode "筛选备份失败"
    Assert-Equal 0 (Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1")).ExitCode "筛选恢复失败"
    Assert-True (-not (Test-Path (Join-Path $paths.Restore "nested\skip.tmp"))) "被排除文件仍被恢复"
    Assert-True (Test-Path (Join-Path $paths.Restore "alpha.txt")) "正常文件未恢复"
    "exclude=skip 生效"
}
Add-Test "TC-042" "文件名通配筛选" "筛选" "仅备份匹配 alpha.??? 的文件" {
    $paths = New-CasePaths "TC-042"
    Assert-Equal 0 (Invoke-Core @("backup", "--source", $script:FolderSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "none", "--glob", "alpha.???")).ExitCode "通配筛选备份失败"
    Assert-Equal 0 (Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1")).ExitCode "通配筛选恢复失败"
    $files = Get-ChildItem -LiteralPath $paths.Restore -File -Recurse
    Assert-Equal 1 $files.Count "alpha.??? 筛选文件数错误"
    Assert-Equal "alpha.txt" $files[0].Name "通配筛选恢复了错误文件"
    "alpha.??? 仅恢复 alpha.txt"
}
Add-Test "TC-043" "条目类型筛选" "筛选" "type=file 时恢复全部七个普通文件" {
    $paths = New-CasePaths "TC-043"
    Assert-Equal 0 (Invoke-Core @("backup", "--source", $script:FolderSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "none", "--type", "file")).ExitCode "类型筛选备份失败"
    Assert-Equal 0 (Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1")).ExitCode "类型筛选恢复失败"
    Assert-Equal 7 (Get-ChildItem -LiteralPath $paths.Restore -File -Recurse).Count "普通文件数量错误"
    "type=file 恢复 7 个普通文件"
}
Add-Test "TC-044" "中文单文件路径" "文件系统" "中文路径和内容可正确备份恢复" {
    $paths = New-CasePaths "TC-044"
    Assert-Equal 0 (Invoke-Core @("backup", "--source", $unicodeSingle, "--output", $paths.Package, "--compression", $preferredCompression, "--encryption", "chacha20_poly1305", "--kdf", "100000") "中文密码-2026").ExitCode "中文路径备份失败"
    Assert-Equal 0 (Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1") "中文密码-2026").ExitCode "中文路径恢复失败"
    $restored = Join-Path $paths.Restore "测试文件.txt"
    Assert-True (Test-Path $restored) "中文文件名未恢复"
    Assert-Equal (Get-FileHash $unicodeSingle -Algorithm SHA256).Hash (Get-FileHash $restored -Algorithm SHA256).Hash "中文文件哈希错误"
    "中文路径和中文密码恢复成功"
}
Add-Test "TC-045" "不存在的备份源" "错误处理" "返回非零退出码和明确错误" {
    $paths = New-CasePaths "TC-045"
    $result = Invoke-Core @("backup", "--source", (Join-Path $paths.Base "missing"), "--output", $paths.Package, "--compression", "stored", "--encryption", "none")
    Assert-True ($result.ExitCode -ne 0) "不存在的源被接受"
    Assert-True ($result.Text -match "不存在") "错误信息不明确"
    "不存在源被拒绝"
}
Add-Test "TC-046" "加密时空密码" "错误处理" "选择加密但无密码时拒绝备份" {
    $paths = New-CasePaths "TC-046"
    $result = Invoke-Core @("backup", "--source", $script:SingleSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "aes256_gcm", "--kdf", "100000")
    Assert-True ($result.ExitCode -ne 0) "空密码加密被接受"
    Assert-True ($result.Text -match "必须填写密码") "空密码错误提示不正确"
    "空密码被拒绝"
}
Add-Test "TC-047" "输出与源文件相同" "错误处理" "拒绝覆盖备份源自身" {
    $result = Invoke-Core @("backup", "--source", $script:SingleSource, "--output", $script:SingleSource, "--compression", "stored", "--encryption", "none")
    Assert-True ($result.ExitCode -ne 0) "允许输出覆盖源文件"
    Assert-True ($result.Text -match "不能与备份源文件相同") "错误提示不正确"
    "源与输出同路径被拒绝"
}
Add-Test "TC-048" "无密码读取 AES 包头" "维护接口" "header 无需密码即可读取非敏感信息" {
    $prepared = New-EncryptedPackage "TC-048" "aes256_gcm"
    $header = Invoke-Core @("header", "--package", $prepared.Paths.Package)
    Assert-Equal 0 $header.ExitCode "读取 AES 包头失败"
    Assert-Equal "aes256_gcm" (Get-CoreResultData $header).encryption "包头加密标识错误"
    "AES 包头读取成功"
}
Add-Test "TC-049" "无密码读取 ChaCha20 包头" "维护接口" "header 无需密码即可读取非敏感信息" {
    $prepared = New-EncryptedPackage "TC-049" "chacha20_poly1305"
    $header = Invoke-Core @("header", "--package", $prepared.Paths.Package)
    Assert-Equal 0 $header.ExitCode "读取 ChaCha20 包头失败"
    Assert-Equal "chacha20_poly1305" (Get-CoreResultData $header).encryption "包头加密标识错误"
    "ChaCha20 包头读取成功"
}
Add-Test "TC-050" "后端维护校验命令" "维护接口" "verify 可供自动化测试使用" {
    $prepared = New-EncryptedPackage "TC-050" "aes256_gcm"
    $verify = Invoke-Core @("verify", "--package", $prepared.Paths.Package) $prepared.Password
    Assert-Equal 0 $verify.ExitCode "维护校验命令失败"
    Assert-Equal 1 (Get-CoreResultData $verify).files "校验文件数错误"
    "verify 维护命令校验 1 个文件成功"
}
Add-Test "TC-051" "非法压缩算法" "参数校验" "未知压缩名称被拒绝" {
    $paths = New-CasePaths "TC-051"
    $result = Invoke-Core @("backup", "--source", $script:SingleSource, "--output", $paths.Package, "--compression", "unknown", "--encryption", "none")
    Assert-True ($result.ExitCode -ne 0) "非法压缩算法被接受"
    "非法压缩算法被拒绝"
}
Add-Test "TC-052" "非法加密算法" "参数校验" "未知加密名称被拒绝" {
    $paths = New-CasePaths "TC-052"
    $result = Invoke-Core @("backup", "--source", $script:SingleSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "unknown")
    Assert-True ($result.ExitCode -ne 0) "非法加密算法被接受"
    "非法加密算法被拒绝"
}
Add-Test "TC-053" "KDF 迭代次数下限" "参数校验" "低于 100000 的 KDF 参数被拒绝" {
    $paths = New-CasePaths "TC-053"
    $result = Invoke-Core @("backup", "--source", $script:SingleSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "aes256_gcm", "--kdf", "99999") "password"
    Assert-True ($result.ExitCode -ne 0) "过低 KDF 次数被接受"
    Assert-True ($result.Text -match "KDF") "KDF 错误提示不明确"
    "KDF=99999 被拒绝"
}
Add-Test "TC-054" "筛选结果为空" "筛选" "无匹配条目时仍生成可恢复空归档" {
    $paths = New-CasePaths "TC-054"
    $backup = Invoke-Core @("backup", "--source", $script:FolderSource, "--output", $paths.Package, "--compression", "stored", "--encryption", "none", "--include", "never-match-token")
    Assert-Equal 0 $backup.ExitCode "空筛选结果备份失败"
    Assert-Equal 0 (Get-CoreResultData $backup).entries "空筛选结果仍含条目"
    $restore = Invoke-Core @("restore", "--package", $paths.Package, "--destination", $paths.Restore, "--overwrite", "1")
    Assert-Equal 0 $restore.ExitCode "空归档恢复失败"
    Assert-Equal 0 (Get-ChildItem -LiteralPath $paths.Restore -File -Recurse).Count "空归档恢复出了文件"
    "0 条目归档创建和恢复成功"
}

$caseCsv = Join-Path $PSScriptRoot "test_cases.csv"
$resultCsv = Join-Path $script:ResultsDir "latest.csv"
$resultJson = Join-Path $script:ResultsDir "latest.json"
$textLog = Join-Path $script:ResultsDir "latest.log"
$eventLog = Join-Path $script:ResultsDir "backend-events.log"
$summaryMd = Join-Path $script:ResultsDir "summary.md"
$script:Definitions | Export-Csv -LiteralPath $caseCsv -NoTypeInformation -Encoding UTF8
$script:Results | Export-Csv -LiteralPath $resultCsv -NoTypeInformation -Encoding UTF8
$script:Results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $resultJson -Encoding UTF8
$script:BackendEvents | Set-Content -LiteralPath $eventLog -Encoding UTF8

$passed = ($script:Results | Where-Object 状态 -eq "通过").Count
$failed = ($script:Results | Where-Object 状态 -eq "失败").Count
$total = $script:Results.Count
$duration = ($script:Results | Measure-Object -Property 耗时毫秒 -Sum).Sum
$logLines = [System.Collections.Generic.List[string]]::new()
$logLines.Add("文件备份软件自动化测试结果")
$logLines.Add("执行时间：$((Get-Date).ToString('yyyy-MM-dd HH:mm:ss'))")
$logLines.Add("测试总数：$total")
$logLines.Add("通过：$passed")
$logLines.Add("失败：$failed")
$logLines.Add("累计用例耗时：$duration ms")
$logLines.Add("")
foreach ($result in $script:Results) {
    $logLines.Add("[$($result.状态)] $($result.编号) $($result.名称) | $($result.耗时毫秒) ms | $($result.实际结果)")
}
$logLines | Set-Content -LiteralPath $textLog -Encoding UTF8

$markdown = [System.Collections.Generic.List[string]]::new()
$markdown.Add("# 自动化测试结果摘要")
$markdown.Add("")
$markdown.Add("- 执行时间：$((Get-Date).ToString('yyyy-MM-dd HH:mm:ss'))")
$markdown.Add("- 测试总数：$total")
$markdown.Add("- 通过：$passed")
$markdown.Add("- 失败：$failed")
$markdown.Add("- 累计用例耗时：$duration ms")
$markdown.Add("")
$markdown.Add("| 编号 | 测试名称 | 分类 | 状态 | 耗时(ms) | 实际结果 |")
$markdown.Add("|---|---|---|---|---:|---|")
foreach ($result in $script:Results) {
    $safeDetail = $result.实际结果.Replace("|", "\\|").Replace("`r", " ").Replace("`n", " ")
    $markdown.Add("| $($result.编号) | $($result.名称) | $($result.分类) | $($result.状态) | $($result.耗时毫秒) | $safeDetail |")
}
$markdown | Set-Content -LiteralPath $summaryMd -Encoding UTF8

Write-Host ""
Write-Host "测试完成：总数=$total，通过=$passed，失败=$failed" -ForegroundColor Cyan
Write-Host "结果日志：$(ConvertTo-ProjectRelativeArgument $textLog)"
if ($failed -gt 0) { exit 1 }
exit 0
