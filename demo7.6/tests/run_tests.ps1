$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Exe = Join-Path $Root "bin/sdb.exe"
$GuiExe = Join-Path $Root "bin/sdb_gui.exe"
$Tmp = Join-Path $Root "tests/tmp"
$Source = Join-Path $Tmp "source"
$Restore = Join-Path $Tmp "restore"
$WrongRestore = Join-Path $Tmp "wrong"
$Archive = Join-Path $Tmp "sample.sdb"

if (Test-Path $Tmp) {
    Remove-Item -Recurse -Force $Tmp
}
if (-not (Test-Path $GuiExe)) {
    throw "GUI executable was not built"
}
New-Item -ItemType Directory -Force (Join-Path $Source "docs") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $Source "empty") | Out-Null

Set-Content -NoNewline -Encoding UTF8 (Join-Path $Source "docs/readme.txt") "aaaaabbbbbcccccc"
Set-Content -NoNewline -Encoding UTF8 (Join-Path $Source "docs/skip.tmp") "temporary"
Set-Content -NoNewline -Encoding UTF8 (Join-Path $Source "root.bin") ([string]::new("Z", 128))

& $Exe backup $Source $Archive --compress --password secret --exclude-ext .tmp
if ($LASTEXITCODE -ne 0) {
    throw "backup failed"
}

$ListOutput = & $Exe list $Archive
if ($LASTEXITCODE -ne 0) {
    throw "list failed"
}
if (($ListOutput -join "`n") -notmatch "docs/readme.txt") {
    throw "list output does not contain expected file"
}

& $Exe restore $Archive $Restore --password secret
if ($LASTEXITCODE -ne 0) {
    throw "restore failed"
}

$Original = Get-Content -Raw (Join-Path $Source "docs/readme.txt")
$Restored = Get-Content -Raw (Join-Path $Restore "docs/readme.txt")
if ($Original -ne $Restored) {
    throw "restored text file differs"
}

$OriginalBin = Get-Content -Raw (Join-Path $Source "root.bin")
$RestoredBin = Get-Content -Raw (Join-Path $Restore "root.bin")
if ($OriginalBin -ne $RestoredBin) {
    throw "restored binary-like file differs"
}

if (Test-Path (Join-Path $Restore "docs/skip.tmp")) {
    throw "excluded .tmp file was restored"
}

if (-not (Test-Path (Join-Path $Restore "empty"))) {
    throw "empty directory was not restored"
}

$PreviousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $Exe restore $Archive $WrongRestore --password wrong 2>$null
$WrongPasswordExitCode = $LASTEXITCODE
$ErrorActionPreference = $PreviousErrorActionPreference
if ($WrongPasswordExitCode -eq 0) {
    throw "wrong password unexpectedly succeeded"
}

Write-Host "All tests passed."
