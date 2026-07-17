/**
 * @file pbackup_main.cpp
 * @brief PBackup 主程序入口 - 动态加载 pbackup_core.dll
 *
 * 本文件是一个轻量级的可执行程序入口，它在运行时动态加载 pbackup_core.dll，
 * 通过 GetProcAddress 获取导出函数指针，然后将命令行参数转发给 DLL 中的
 * pbackup_execute 函数执行。
 *
 * 架构说明：
 * ┌─────────────────┐         ┌──────────────────────┐
 * │ pbackup_core.exe│ ──────> │   pbackup_core.dll   │
 * │ (本文件编译)     │ LoadLib │ (核心逻辑动态链接库)   │
 * └─────────────────┘         └──────────────────────┘
 *
 * 这种设计的优点：
 * 1. 核心逻辑可被多个程序复用（exe、Python ctypes、测试工具等）
 * 2. 可以在不重新编译主程序的情况下更新核心逻辑
 * 3. 符合"动态链接"实验要求
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  /* 最低要求 Windows 7 */
#endif

#include <windows.h>
#include <iostream>
#include <string>

/* ─── DLL 导出函数的类型定义 ─── */

/** pbackup_execute 函数指针类型：接收命令行参数，返回退出码 */
typedef int (*PBackupExecuteFn)(int argc, wchar_t** argv);

/** pbackup_cancel 函数指针类型：请求取消当前操作 */
typedef void (*PBackupCancelFn)(void);

/** pbackup_version 函数指针类型：返回版本字符串 */
typedef const char* (*PBackupVersionFn)(void);

/**
 * @brief 获取 DLL 路径
 *
 * DLL 与本 exe 放在同一目录下，通过获取 exe 的完整路径来推导 DLL 路径。
 * 这样无论从哪个工作目录启动程序，都能正确找到 DLL。
 *
 * @return DLL 的完整路径
 */
static std::wstring get_dll_path() {
    /* 获取当前可执行文件的完整路径 */
    wchar_t exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);

    /* 将文件名部分替换为 DLL 名称 */
    std::wstring path(exe_path);
    const auto last_slash = path.find_last_of(L"\\/");
    if (last_slash != std::wstring::npos) {
        path = path.substr(0, last_slash + 1);
    } else {
        path = L".\\";
    }
    path += L"pbackup_core.dll";
    return path;
}

/**
 * @brief 标准取消监听线程
 *
 * 在后台线程中监听 stdin，当收到 "cancel" 行时调用 DLL 的取消函数。
 * 这允许 Python GUI 通过管道向后端发送取消信号。
 *
 * @param cancel_fn 取消函数指针
 */
static void start_cancel_listener(PBackupCancelFn cancel_fn) {
    /* 创建分离线程监听标准输入 */
    HANDLE thread = CreateThread(nullptr, 0, [](LPVOID param) -> DWORD {
        auto fn = reinterpret_cast<PBackupCancelFn>(param);
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "cancel") {
                fn();  /* 通知 DLL 取消当前操作 */
                break;
            }
        }
        return 0;
    }, reinterpret_cast<LPVOID>(cancel_fn), 0, nullptr);

    if (thread) {
        CloseHandle(thread);  /* 分离线程，不需要等待 */
    }
}

/**
 * @brief 程序入口点 (Unicode 版本)
 *
 * 流程：
 * 1. 设置控制台编码为 UTF-8
 * 2. 动态加载 pbackup_core.dll
 * 3. 获取导出函数地址
 * 4. 启动取消监听线程
 * 5. 将命令行参数转发给 DLL 执行
 * 6. 返回 DLL 的退出码
 */
int wmain(int argc, wchar_t** argv) {
    /* 设置控制台输入输出编码为 UTF-8，确保中文正确显示 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::ios::sync_with_stdio(false);

    /* 检查命令行参数：至少需要一个命令名 */
    if (argc < 2) {
        std::cerr << "{\"type\":\"error\",\"message\":\"缺少命令。"
                     "可用命令：capabilities、backup、restore、verify、compare、header。\"}\n";
        return 1;
    }

    /* ─── 步骤1：定位并加载 DLL ─── */
    const std::wstring dll_path = get_dll_path();
    HMODULE dll = LoadLibraryW(dll_path.c_str());
    if (!dll) {
        const DWORD err = GetLastError();
        std::cerr << "{\"type\":\"error\",\"message\":\"无法加载 pbackup_core.dll"
                     "（错误代码 " << err << "）。"
                     "请确保 DLL 与 exe 在同一目录下。\"}\n";
        return 1;
    }

    /* ─── 步骤2：获取导出函数地址 ─── */
    auto execute_fn = reinterpret_cast<PBackupExecuteFn>(
        GetProcAddress(dll, "pbackup_execute"));
    auto cancel_fn = reinterpret_cast<PBackupCancelFn>(
        GetProcAddress(dll, "pbackup_cancel"));
    auto version_fn = reinterpret_cast<PBackupVersionFn>(
        GetProcAddress(dll, "pbackup_version"));

    if (!execute_fn || !cancel_fn) {
        std::cerr << "{\"type\":\"error\",\"message\":\"pbackup_core.dll "
                     "缺少必需的导出函数。\"}\n";
        FreeLibrary(dll);
        return 1;
    }

    /* ─── 步骤3：如果请求版本信息，直接输出 ─── */
    if (version_fn && std::wstring(argv[1]) == L"--version") {
        std::cout << version_fn() << std::endl;
        FreeLibrary(dll);
        return 0;
    }

    /* ─── 步骤4：启动取消监听线程 ─── */
    start_cancel_listener(cancel_fn);

    /* ─── 步骤5：将参数转发给 DLL 执行 ───
     * 注意：跳过 argv[0]（exe路径），从 argv[1] 开始传递
     * DLL 的 pbackup_execute 接收的 argv[0] 是命令名（如 "backup"）
     */
    int result = execute_fn(argc - 1, argv + 1);

    /* ─── 步骤6：清理并返回 ─── */
    FreeLibrary(dll);
    return result;
}
