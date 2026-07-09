@echo off
chcp 65001 >nul
setlocal
rem ============================================================
rem  一键重建：Qt 版每次构建需要“全英文路径”的镜像目录，
rem  本脚本把课程文件夹里的源码复制到 G:\_src（英文路径），
rem  然后用 CMake + Ninja(MSVC) 构建，产物在 G:\_bmsvc。
rem  课程文件夹(F:) 才是源码权威副本；G: 只是可随时重建的中转。
rem ============================================================

set "COURSE=%~dp0"
set "SRC_MIRROR=G:\_src"
set "BUILD_DIR=G:\_bmsvc"

echo [1/4] 清理旧镜像与构建目录...
if exist "%SRC_MIRROR%" rmdir /s /q "%SRC_MIRROR%"
if exist "%BUILD_DIR%"  rmdir /s /q "%BUILD_DIR%"
mkdir "%SRC_MIRROR%"

echo [2/4] 复制源码到英文镜像目录 %SRC_MIRROR% ...
xcopy "%COURSE%gui\BackupTool" "%SRC_MIRROR%" /E /I /Y /Q >nul

echo [3/4] 配置 CMake (Ninja + MSVC)...
where cl >nul 2>nul || call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
cmake -S "%SRC_MIRROR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (echo [X] CMake 配置失败 & pause & exit /b 1)

echo [4/4] 编译...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (echo [X] 构建失败 & pause & exit /b 1)

echo.
echo [OK] 构建完成，exe 位于 %BUILD_DIR%\BackupTool.exe
echo      可运行 tools\setup_local_run.py 把它部署到课程 bin\ 并生成启动脚本。
pause
