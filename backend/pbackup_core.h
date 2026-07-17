/**
 * @file pbackup_core.h
 * @brief PBackup 核心动态链接库的导出接口声明
 *
 * 本头文件定义了 pbackup_core.dll 的公共 API。
 * 主程序 (pbackup_core.exe) 和任何第三方程序均可通过此接口调用备份核心功能。
 *
 * 设计原则：
 * - 使用 C 语言链接 (extern "C") 以确保跨编译器兼容
 * - 所有字符串参数使用 UTF-8 编码
 * - 通过回调函数向调用者报告进度、日志和结果
 */

#ifndef PBACKUP_CORE_H
#define PBACKUP_CORE_H

#ifdef _WIN32
    #ifdef PBACKUP_BUILDING_DLL
        /** 导出符号：编译 DLL 时定义此宏 */
        #define PBACKUP_API __declspec(dllexport)
    #else
        /** 导入符号：使用 DLL 时不定义 PBACKUP_BUILDING_DLL */
        #define PBACKUP_API __declspec(dllimport)
    #endif
#else
    #define PBACKUP_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 执行一条备份核心命令
 *
 * 此函数是 DLL 的主入口点。它接收命令行参数（与原 exe 格式一致），
 * 在内部执行相应的备份/恢复/验证操作，并通过 stdout 输出 JSON Lines。
 *
 * @param argc 参数个数（包含命令名）
 * @param argv 宽字符参数数组，argv[0] 为命令名（如 "backup"、"restore"）
 * @return int 返回码：0=成功, 2=业务错误, 3=文件系统错误, 4=其他异常
 *
 * 示例用法：
 *   wchar_t* args[] = { L"backup", L"--source", L"C:\\data", L"--output", L"C:\\out.pbk" };
 *   int ret = pbackup_execute(5, args);
 */
PBACKUP_API int pbackup_execute(int argc, wchar_t** argv);

/**
 * @brief 请求取消当前正在执行的操作
 *
 * 调用此函数后，核心会在下一个检查点抛出取消异常并安全退出。
 * 线程安全：可以从任意线程调用。
 */
PBACKUP_API void pbackup_cancel(void);

/**
 * @brief 获取库版本信息
 * @return 静态分配的版本字符串，如 "0.7.2"
 */
PBACKUP_API const char* pbackup_version(void);

#ifdef __cplusplus
}
#endif

#endif /* PBACKUP_CORE_H */
