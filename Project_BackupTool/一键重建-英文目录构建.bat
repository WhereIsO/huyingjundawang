@echo off
chcp 65001 >nul
setlocal
rem 兼容旧入口：实际构建逻辑在 gui\BackupTool\build_msvc.bat。
rem 默认镜像目录为 %TEMP%\BackupTool_src，构建目录为 %TEMP%\BackupTool_build。
rem 如需指定 Qt，请先设置 QT_PREFIX 或 CMAKE_PREFIX_PATH。

pushd "%~dp0gui\BackupTool"
call build_msvc.bat
set "RESULT=%ERRORLEVEL%"
popd

if not "%RESULT%"=="0" (
    echo [X] 构建失败
    pause
    exit /b %RESULT%
)

echo [OK] 构建完成。可执行文件位置见上方 build_msvc.bat 输出。
pause
