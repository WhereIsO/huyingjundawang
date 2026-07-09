param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [string]$PackageDir = "build\package\BackupTool-windows-x64",
    [string]$ZipPath = "build\BackupTool-windows-x64.zip"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$buildPath = (Resolve-Path -LiteralPath (Join-Path $repoRoot $BuildDir)).Path

$exeCandidates = @(
    (Join-Path $buildPath "$Config\BackupTool.exe"),
    (Join-Path $buildPath "BackupTool.exe")
)
$sourceExe = $exeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $sourceExe) {
    throw "BackupTool.exe was not found under $buildPath"
}

$packagePath = Join-Path $repoRoot $PackageDir
if (Test-Path -LiteralPath $packagePath) {
    Remove-Item -LiteralPath $packagePath -Recurse -Force
}
New-Item -ItemType Directory -Path $packagePath | Out-Null

$destExe = Join-Path $packagePath "BackupTool.exe"
Copy-Item -LiteralPath $sourceExe -Destination $destExe -Force

$windeployqt = $null
if ($env:QT_PREFIX) {
    $candidate = Join-Path $env:QT_PREFIX "bin\windeployqt.exe"
    if (Test-Path -LiteralPath $candidate) {
        $windeployqt = $candidate
    }
}
if (-not $windeployqt) {
    $command = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($command) {
        $windeployqt = $command.Source
    }
}
if (-not $windeployqt) {
    throw "windeployqt.exe was not found. Set QT_PREFIX to the Qt installation root or add Qt bin to PATH."
}

$vcvars = $null
if (-not $env:VCINSTALLDIR) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstall) {
            $candidate = Join-Path $vsInstall "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path -LiteralPath $candidate) {
                $vcvars = $candidate
            }
        }
    }
}

if ($vcvars) {
    $deployCmd = "`"$vcvars`" && `"$windeployqt`" --release --compiler-runtime --dir `"$packagePath`" `"$destExe`""
    cmd /c $deployCmd
} else {
    & $windeployqt --release --compiler-runtime --dir $packagePath $destExe
}
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

$readme = @"
BackupTool Windows x64 package

Run BackupTool.exe to start the application.
This package is generated from the CMake Release build and includes Qt runtime files collected by windeployqt.
"@
Set-Content -LiteralPath (Join-Path $packagePath "README.txt") -Value $readme -Encoding UTF8

$zipFullPath = Join-Path $repoRoot $ZipPath
$zipParent = Split-Path -Parent $zipFullPath
if (-not (Test-Path -LiteralPath $zipParent)) {
    New-Item -ItemType Directory -Path $zipParent | Out-Null
}
if (Test-Path -LiteralPath $zipFullPath) {
    Remove-Item -LiteralPath $zipFullPath -Force
}
Compress-Archive -Path (Join-Path $packagePath "*") -DestinationPath $zipFullPath -Force
Write-Host "Package directory: $packagePath"
Write-Host "Package zip: $zipFullPath"
