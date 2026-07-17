/**
 * @file pbackup_core.cpp
 * @brief PBackup 核心动态链接库实现
 *
 * 本文件包含 PBackup 备份软件的全部核心逻辑，编译为 pbackup_core.dll。
 * 主要功能模块：
 *
 * 1. 编码转换 - UTF-8 与 UTF-16 (Windows 宽字符) 的互转
 * 2. JSON 输出 - 通过 JSON Lines 协议向前端报告进度和结果
 * 3. 加密模块 - AES-256-GCM 和 ChaCha20-Poly1305 认证加密
 * 4. 压缩模块 - 动态加载 Cabinet.dll 实现 MSZIP/XPRESS/LZMS 压缩
 * 5. 备份归档 - 自定义二进制归档格式的读写
 * 6. 文件扫描 - 递归扫描目录并应用过滤规则
 * 7. 备份验证 - 校验备份包完整性和与源文件的一致性比对
 *
 * 安全设计：
 * - 密钥材料在使用后通过 SecureZeroMemory 清零
 * - PBKDF2-HMAC-SHA256 密钥派生，默认 60 万次迭代
 * - 路径安全校验防止目录穿越攻击
 * - AEAD 认证加密确保数据完整性和真实性
 * - 原子文件写入防止断电导致半写文件
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  /* 最低要求 Windows 7 */
#endif

/* 标记为 DLL 编译模式，使头文件中的宏展开为 __declspec(dllexport) */
#ifndef PBACKUP_BUILDING_DLL
#define PBACKUP_BUILDING_DLL
#endif
#include "pbackup_core.h"

#include <windows.h>   /* Windows API 核心头文件 */
#include <bcrypt.h>    /* Windows CNG 加密 API (SHA-256, AES-GCM, PBKDF2, 随机数) */

#include <algorithm>   /* std::sort, std::min, std::transform 等算法 */
#include <array>       /* std::array 固定大小数组 */
#include <atomic>      /* std::atomic_bool 线程安全取消标志 */
#include <chrono>      /* 时间处理，用于时间戳转换 */
#include <cstdint>     /* uint8_t, uint32_t, uint64_t 等定宽整数类型 */
#include <cstring>     /* memcpy, memset 等内存操作 */

#include <filesystem> /* C++17 文件系统库：目录遍历、路径操作、文件状态 */

#include <fstream>    /* 文件流：读写备份包和临时文件 */
#include <iomanip>    /* 格式化输出：十六进制、时间格式化 */
#include <iostream>   /* 标准输入输出：JSON Lines 事件输出 */
#include <map>        /* 有序映射：参数解析、哈希比对 */
#include <memory>     /* 智能指针：RAII 资源管理 */
#include <optional>   /* 可选值：筛选条件可能为空 */
#include <set>        /* 有序集合：条目类型筛选 */
#include <sstream>    /* 字符串流：构建 JSON 输出 */
#include <stdexcept>  /* 异常基类：CoreError 继承自 runtime_error */
#include <string>     /* 字符串：UTF-8 路径和消息 */
#include <thread>     /* 线程支持（备用） */
#include <vector>     /* 动态数组：数据缓冲区 */

/** @brief C++17 文件系统命名空间别名 */
namespace fs = std::filesystem;

/**
 * @brief 匿名命名空间：所有内部实现细节
 *
 * 使用匿名命名空间而非 static 关键字来限制符号可见性，
 * 这是 C++ 中推荐的内部链接方式。
 */
namespace {

/* ═══════════════════════════════════════════════════════════════════════════
 * 常量定义
 * ═══════════════════════════════════════════════════════════════════════════ */

/** I/O 缓冲区大小：1MB
 *  选择 1MB 的理由：
 *  - 足够大以减少系统调用次数
 *  - 足够小以避免大文件整体载入内存
 *  - 与 AEAD 分帧大小一致，每帧独立认证
 */
constexpr std::size_t IO_CHUNK = 1024 * 1024;

/** 备份包格式版本号 */
constexpr std::uint32_t FORMAT_VERSION = 3;

/** PBKDF2 默认迭代次数：60 万次
 *  OWASP 2023 推荐 SHA-256 至少 600,000 次迭代
 *  迭代次数越高越安全，但备份/恢复时密钥派生耗时也越长
 */
constexpr std::uint32_t DEFAULT_KDF_ITERATIONS = 600000;
constexpr std::array<unsigned char, 16> PACKAGE_MAGIC = {
    'P', 'B', 'A', 'C', 'K', 'U', 'P', '-', 'C', 'P', 'P', '3', '\r', '\n', 0, 0};
constexpr std::array<unsigned char, 8> ARCHIVE_MAGIC = {'P', 'B', 'A', 'R', 'C', 'P', 'P', '3'};
constexpr std::size_t PACKAGE_HEADER_SIZE = 116;

/** 全局取消标志（原子变量）
 *  GUI 通过 stdin 发送 "cancel" → 主程序调用 pbackup_cancel()
 *  → 设置此标志为 true → 各处 check_cancelled() 检查并抛出异常
 */
std::atomic_bool g_cancelled{false};

/* ─── Windows Compression API 函数指针类型定义 ───
 * 由于 Cabinet.dll 通过 LoadLibrary 动态加载，需要手动声明函数指针类型
 */
using CompressionHandle = PVOID;  // 压缩器/解压器句柄
using CreateCompressorFn = BOOL (WINAPI *)(DWORD, PVOID, CompressionHandle*);   // 创建压缩器
using CloseCompressorFn = BOOL (WINAPI *)(CompressionHandle);                   // 关闭压缩器
using CompressFn = BOOL (WINAPI *)(CompressionHandle, PVOID, SIZE_T, PVOID, SIZE_T, SIZE_T*);  // 压缩数据
using CreateDecompressorFn = BOOL (WINAPI *)(DWORD, PVOID, CompressionHandle*); // 创建解压器
using CloseDecompressorFn = BOOL (WINAPI *)(CompressionHandle);                 // 关闭解压器
using DecompressFn = BOOL (WINAPI *)(CompressionHandle, PVOID, SIZE_T, PVOID, SIZE_T, SIZE_T*); // 解压数据

/* Windows Compression API 算法标识常量（与 compressapi.h 中定义一致） */
constexpr DWORD COMPRESS_ALGORITHM_MSZIP_VALUE = 2;       // MSZIP (Deflate)
constexpr DWORD COMPRESS_ALGORITHM_XPRESS_VALUE = 3;      // XPRESS
constexpr DWORD COMPRESS_ALGORITHM_XPRESS_HUFF_VALUE = 4; // XPRESS + Huffman
constexpr DWORD COMPRESS_ALGORITHM_LZMS_VALUE = 5;        // LZMS

/** 业务异常类
 *  所有可预期的业务错误（密码错误、文件不存在、格式无效等）
 *  都抛出此异常，由顶层 pbackup_execute 统一捕获并转为 JSON 错误事件
 */
class CoreError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;  // 继承父类构造函数
};

/** 检查取消标志，若已取消则抛出异常终止当前操作 */
void check_cancelled() {
    if (g_cancelled.load()) {  // 原子读取取消标志
        throw CoreError("任务已取消。");
    }
}

/** 将 UTF-16 宽字符串转换为 UTF-8 字符串
 *  Windows 内部使用 UTF-16，但备份包和 JSON 输出使用 UTF-8
 */
std::string utf8_from_wide(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw CoreError("UTF-16 路径转换失败。");
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

/** 将 UTF-8 字符串转换为 UTF-16 宽字符串
 *  用于将备份包中的路径还原为 Windows 文件系统可识别的格式
 */
std::wstring wide_from_utf8(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        throw CoreError("备份包包含无效的 UTF-8 路径。");
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

/** 对字符串进行 JSON 转义，处理引号、反斜杠和控制字符 */
std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch) << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

/** 将字符串用双引号包裹并转义，用于 JSON 值输出 */
std::string quote(const std::string& value) {
    return "\"" + json_escape(value) + "\"";
}

/** 向 stdout 输出一行 JSON 并立即刷新
 *  GUI 通过 stdout 管道实时读取这些事件
 */
void emit_json(const std::string& json) {
    std::cout << json << '\n' << std::flush;  // flush 确保 GUI 能立即收到
}

/** 输出日志事件（level: info/warn/error/ok） */
void emit_log(const std::string& level, const std::string& message) {
    emit_json("{\"type\":\"log\",\"level\":" + quote(level) + ",\"message\":" + quote(message) + "}");
}

/** 输出进度事件，包含阶段、文件数、字节数和当前路径 */
void emit_progress(
    const std::string& stage,
    std::uint64_t total_files,
    std::uint64_t done_files,
    std::uint64_t total_bytes,
    std::uint64_t done_bytes,
    const std::string& current_path,
    const std::string& message) {
    std::ostringstream out;
    out << "{\"type\":\"progress\",\"stage\":" << quote(stage)
        << ",\"total_files\":" << total_files
        << ",\"done_files\":" << done_files
        << ",\"total_bytes\":" << total_bytes
        << ",\"done_bytes\":" << done_bytes
        << ",\"current_path\":" << quote(current_path)
        << ",\"message\":" << quote(message) << "}";
    emit_json(out.str());
}

/** 将 Windows 错误码格式化为可读的中文错误消息
 *  使用 FormatMessageW 获取系统错误描述，附加到自定义前缀后
 */
std::string windows_error(const std::string& prefix, DWORD code = GetLastError()) {
    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    std::string message = prefix;
    if (length && buffer) {
        std::wstring text(buffer, length);
        LocalFree(buffer);
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ')) {
            text.pop_back();
        }
        message += "：" + utf8_from_wide(text);
    } else {
        message += "（错误代码 " + std::to_string(code) + "）";
    }
    return message;
}

/** 检查 NTSTATUS 返回值，失败时抛出异常
 *  Windows CNG API (bcrypt.dll) 使用 NTSTATUS 报告错误
 */
void check_nt(NTSTATUS status, const std::string& action) {
    if (status < 0) {
        std::ostringstream out;
        out << action << "失败（NTSTATUS 0x" << std::hex << static_cast<unsigned long>(status) << "）";
        throw CoreError(out.str());
    }
}

/** 以小端序写入一个整数到输出流
 *  备份包中所有多字节整数都使用小端序（与 x86 内存布局一致）
 */
template <typename T>
void write_le(std::ostream& out, T value) {
    using U = typename std::make_unsigned<T>::type;
    U unsigned_value = static_cast<U>(value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.put(static_cast<char>((unsigned_value >> (i * 8)) & 0xff));
    }
    if (!out) {
        throw CoreError("写入备份数据失败。");
    }
}

/** 从输入流读取一个小端序整数
 *  遇到 EOF 时抛出异常，确保不会读取不完整的数据
 */
template <typename T>
T read_le(std::istream& in) {
    using U = typename std::make_unsigned<T>::type;
    U value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        const int ch = in.get();
        if (ch == EOF) {
            throw CoreError("备份包意外结束。");
        }
        value |= static_cast<U>(static_cast<unsigned char>(ch)) << (i * 8);
    }
    return static_cast<T>(value);
}

/** 向输出流写入指定长度的原始字节数据 */
void write_bytes(std::ostream& out, const void* data, std::size_t size) {
    if (size == 0) {
        return;
    }
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    if (!out) {
        throw CoreError("写入备份数据失败。");
    }
}

/** 从输入流精确读取指定长度的字节，不足则抛出异常 */
void read_bytes(std::istream& in, void* data, std::size_t size) {
    if (size == 0) {
        return;
    }
    in.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
    if (static_cast<std::size_t>(in.gcount()) != size) {
        throw CoreError("备份包意外结束。");
    }
}

/** 生成 8 字节随机十六进制字符串，用于临时文件名后缀 */
std::string random_suffix() {
    std::array<unsigned char, 8> bytes{};
    check_nt(BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG), "生成随机数");
    std::ostringstream out;
    for (const auto byte : bytes) {
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return out.str();
}

/** RAII 临时文件管理器
 *  构造时记录路径，析构时自动删除（除非调用 release() 表示保留）
 *  用于确保异常或取消时不留下半成品文件
 */
class TempPath {
public:
    explicit TempPath(fs::path path) : path_(std::move(path)) {}
    ~TempPath() {
        if (!released_) {
            std::error_code ec;
            fs::remove(path_, ec);
        }
    }
    const fs::path& path() const { return path_; }
    void release() { released_ = true; }

private:
    fs::path path_;
    bool released_ = false;
};

/** 在目标文件同目录创建临时文件路径（确保原子替换在同一文件系统） */
fs::path sibling_temp(const fs::path& target) {
    return target.parent_path() / (target.filename().wstring() + L".tmp." + wide_from_utf8(random_suffix()));
}

/** 在系统临时目录创建临时文件路径 */
fs::path system_temp(const std::wstring& prefix) {
    return fs::temp_directory_path() / (prefix + wide_from_utf8(random_suffix()) + L".tmp");
}

/** 原子替换目标文件
 *  使用 MoveFileExW + MOVEFILE_REPLACE_EXISTING 确保：
 *  - 要么完整替换成功
 *  - 要么目标文件保持不变（不会出现半写状态）
 */
void atomic_replace(const fs::path& temp, const fs::path& target) {
    std::error_code ec;
    if (!target.parent_path().empty()) {
        fs::create_directories(target.parent_path(), ec);
        if (ec) {
            throw CoreError("无法创建输出目录：" + utf8_from_wide(target.parent_path().wstring()));
        }
    }
    if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw CoreError(windows_error("无法原子替换输出文件"));
    }
}

/** SHA-256 哈希计算封装类
 *  使用 Windows CNG (bcrypt.dll) 实现，RAII 管理算法和哈希句柄
 *  用法：创建 → 多次 update() → 一次 finish() 获取 32 字节摘要
 */
class Hash256 {
public:
    /** 构造函数：初始化 SHA-256 算法提供者和哈希对象 */
    Hash256() {
        check_nt(BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0), "打开 SHA-256");
        DWORD result = 0;
        DWORD size = 0;
        check_nt(BCryptGetProperty(algorithm_, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size), sizeof(size), &result, 0), "读取 SHA-256 参数");
        object_.resize(size);
        check_nt(BCryptCreateHash(algorithm_, &hash_, object_.data(), static_cast<ULONG>(object_.size()), nullptr, 0, 0), "创建 SHA-256");
    }

    Hash256(const Hash256&) = delete;
    Hash256& operator=(const Hash256&) = delete;

    ~Hash256() {
        if (hash_) BCryptDestroyHash(hash_);
        if (algorithm_) BCryptCloseAlgorithmProvider(algorithm_, 0);
    }

    void update(const unsigned char* data, std::size_t size) {
        if (finished_) throw CoreError("SHA-256 已结束。");
        while (size > 0) {
            const ULONG part = static_cast<ULONG>(std::min<std::size_t>(size, 0xffffffffu));
            check_nt(BCryptHashData(hash_, const_cast<PUCHAR>(data), part, 0), "计算 SHA-256");
            data += part;
            size -= part;
        }
    }

    std::array<unsigned char, 32> finish() {
        if (finished_) return digest_;
        check_nt(BCryptFinishHash(hash_, digest_.data(), static_cast<ULONG>(digest_.size()), 0), "完成 SHA-256");
        finished_ = true;
        return digest_;
    }

private:
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;
    std::vector<unsigned char> object_;
    std::array<unsigned char, 32> digest_{};
    bool finished_ = false;
};

/** 常量时间比较两个 SHA-256 摘要
 *  防止侧信道攻击：不论第几字节不同，耗时都相同
 */
bool secure_equal(const std::array<unsigned char, 32>& left, const std::array<unsigned char, 32>& right) {
    unsigned char difference = 0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        difference |= left[i] ^ right[i];
    }
    return difference == 0;
}

/** 压缩算法枚举
 *  值与备份包头中的压缩字段一一对应，不可随意更改
 */
enum class Compression : std::uint32_t {
    Stored = 0,      // 不压缩，直接存储原始数据
    Mszip = 1,       // MSZIP (Deflate)，通用压缩
    Xpress = 2,      // XPRESS，快速压缩
    XpressHuff = 3,  // XPRESS + Huffman，速度与压缩率平衡
    Lzms = 4,        // LZMS，最高压缩率但最慢
};

/** 加密算法枚举
 *  值与备份包头中的加密字段一一对应
 *  两种算法都是 AEAD（认证加密），提供机密性+完整性+真实性
 */
enum class Encryption : std::uint32_t {
    None = 0,              // 不加密
    Aes256Gcm = 1,         // AES-256-GCM（基于 Windows CNG 实现）
    ChaCha20Poly1305 = 2,  // ChaCha20-Poly1305（纯 C++ 实现 RFC 8439）
};

std::string compression_key(Compression value) {
    switch (value) {
    case Compression::Stored: return "stored";
    case Compression::Mszip: return "mszip";
    case Compression::Xpress: return "xpress";
    case Compression::XpressHuff: return "xpress_huff";
    case Compression::Lzms: return "lzms";
    }
    throw CoreError("备份包包含未知压缩方式。");
}

Compression parse_compression(const std::wstring& value) {
    if (value == L"stored") return Compression::Stored;
    if (value == L"mszip") return Compression::Mszip;
    if (value == L"xpress") return Compression::Xpress;
    if (value == L"xpress_huff") return Compression::XpressHuff;
    if (value == L"lzms") return Compression::Lzms;
    throw CoreError("未知压缩方式：" + utf8_from_wide(value));
}

std::string encryption_key(Encryption value) {
    switch (value) {
    case Encryption::None: return "none";
    case Encryption::Aes256Gcm: return "aes256_gcm";
    case Encryption::ChaCha20Poly1305: return "chacha20_poly1305";
    }
    throw CoreError("备份包包含未知加密方式。");
}

Encryption parse_encryption(const std::wstring& value) {
    if (value == L"none") return Encryption::None;
    if (value == L"aes256_gcm") return Encryption::Aes256Gcm;
    if (value == L"chacha20_poly1305") return Encryption::ChaCha20Poly1305;
    throw CoreError("未知加密方式：" + utf8_from_wide(value));
}

DWORD compressor_algorithm(Compression value) {
    switch (value) {
    case Compression::Mszip: return COMPRESS_ALGORITHM_MSZIP_VALUE;
    case Compression::Xpress: return COMPRESS_ALGORITHM_XPRESS_VALUE;
    case Compression::XpressHuff: return COMPRESS_ALGORITHM_XPRESS_HUFF_VALUE;
    case Compression::Lzms: return COMPRESS_ALGORITHM_LZMS_VALUE;
    case Compression::Stored: break;
    }
    throw CoreError("无压缩模式不需要压缩器。");
}

/** Windows Compression API 动态加载单例
 *  在首次使用时加载 Cabinet.dll 并获取所有压缩/解压函数地址
 *  若 DLL 不可用（极旧的 Windows 版本），则 available() 返回 false
 *  使用单例模式确保整个程序生命周期只加载一次
 */
class CompressionApi {
public:
    /** 获取全局唯一实例（线程安全的 C++11 静态局部变量初始化） */
    static const CompressionApi& instance() {
        static const CompressionApi api;
        return api;
    }

    bool available() const { return module_ != nullptr; }

    BOOL create_compressor(DWORD algorithm, CompressionHandle* handle) const {
        return create_compressor_(algorithm, nullptr, handle);
    }

    BOOL close_compressor(CompressionHandle handle) const {
        return close_compressor_(handle);
    }

    BOOL compress(CompressionHandle handle, PVOID input, SIZE_T input_size, PVOID output, SIZE_T output_size, SIZE_T* written) const {
        return compress_(handle, input, input_size, output, output_size, written);
    }

    BOOL create_decompressor(DWORD algorithm, CompressionHandle* handle) const {
        return create_decompressor_(algorithm, nullptr, handle);
    }

    BOOL close_decompressor(CompressionHandle handle) const {
        return close_decompressor_(handle);
    }

    BOOL decompress(CompressionHandle handle, PVOID input, SIZE_T input_size, PVOID output, SIZE_T output_size, SIZE_T* written) const {
        return decompress_(handle, input, input_size, output, output_size, written);
    }

private:
    CompressionApi() {
        module_ = LoadLibraryW(L"Cabinet.dll");
        if (!module_) return;

        create_compressor_ = reinterpret_cast<CreateCompressorFn>(GetProcAddress(module_, "CreateCompressor"));
        close_compressor_ = reinterpret_cast<CloseCompressorFn>(GetProcAddress(module_, "CloseCompressor"));
        compress_ = reinterpret_cast<CompressFn>(GetProcAddress(module_, "Compress"));
        create_decompressor_ = reinterpret_cast<CreateDecompressorFn>(GetProcAddress(module_, "CreateDecompressor"));
        close_decompressor_ = reinterpret_cast<CloseDecompressorFn>(GetProcAddress(module_, "CloseDecompressor"));
        decompress_ = reinterpret_cast<DecompressFn>(GetProcAddress(module_, "Decompress"));
        if (create_compressor_ && close_compressor_ && compress_ &&
            create_decompressor_ && close_decompressor_ && decompress_) {
            return;
        }

        FreeLibrary(module_);
        module_ = nullptr;
        create_compressor_ = nullptr;
        close_compressor_ = nullptr;
        compress_ = nullptr;
        create_decompressor_ = nullptr;
        close_decompressor_ = nullptr;
        decompress_ = nullptr;
    }

    HMODULE module_ = nullptr;
    CreateCompressorFn create_compressor_ = nullptr;
    CloseCompressorFn close_compressor_ = nullptr;
    CompressFn compress_ = nullptr;
    CreateDecompressorFn create_decompressor_ = nullptr;
    CloseDecompressorFn close_decompressor_ = nullptr;
    DecompressFn decompress_ = nullptr;
};

/** 检查当前系统是否支持请求的压缩算法 */
void require_compression_support(Compression method) {
    if (method != Compression::Stored && !CompressionApi::instance().available()) {
        throw CoreError("当前 Windows 版本不支持 MSZIP、XPRESS 和 LZMS 压缩；请使用 stored 无压缩模式。");
    }
}

/** 压缩器 RAII 封装
 *  构造时创建 Windows 压缩句柄，析构时自动关闭
 *  Stored 模式不创建系统压缩器（直接返回原始数据）
 */
class Compressor {
public:
    /** 构造函数：根据压缩方式创建对应的系统压缩器 */
    explicit Compressor(Compression method) : method_(method) {
        require_compression_support(method_);
        if (method_ != Compression::Stored && !CompressionApi::instance().create_compressor(compressor_algorithm(method_), &handle_)) {
            throw CoreError(windows_error("创建压缩器失败"));
        }
    }
    ~Compressor() { if (handle_) CompressionApi::instance().close_compressor(handle_); }

    std::vector<unsigned char> compress(const std::vector<unsigned char>& input) {
        if (method_ == Compression::Stored) return input;
        SIZE_T needed = 0;
        if (CompressionApi::instance().compress(handle_, const_cast<unsigned char*>(input.data()), input.size(), nullptr, 0, &needed)) {
            throw CoreError("压缩器返回了异常的空输出。");
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || needed == 0) {
            throw CoreError(windows_error("计算压缩缓冲区大小失败"));
        }
        std::vector<unsigned char> output(needed);
        SIZE_T written = 0;
        if (!CompressionApi::instance().compress(handle_, const_cast<unsigned char*>(input.data()), input.size(), output.data(), output.size(), &written)) {
            throw CoreError(windows_error("压缩文件数据失败"));
        }
        output.resize(written);
        return output;
    }

private:
    Compression method_;
    CompressionHandle handle_ = nullptr;
};

/** 解压器 RAII 封装
 *  构造时创建 Windows 解压句柄，析构时自动关闭
 *  Stored 模式不创建系统解压器
 */
class Decompressor {
public:
    /** 构造函数：根据压缩方式创建对应的系统解压器 */
    explicit Decompressor(Compression method) : method_(method) {
        require_compression_support(method_);
        if (method_ != Compression::Stored && !CompressionApi::instance().create_decompressor(compressor_algorithm(method_), &handle_)) {
            throw CoreError(windows_error("创建解压器失败"));
        }
    }
    ~Decompressor() { if (handle_) CompressionApi::instance().close_decompressor(handle_); }

    std::vector<unsigned char> decompress(const std::vector<unsigned char>& input, std::size_t expected) {
        if (method_ == Compression::Stored) return input;
        std::vector<unsigned char> output(expected);
        SIZE_T written = 0;
        if (!CompressionApi::instance().decompress(handle_, const_cast<unsigned char*>(input.data()), input.size(), output.data(), output.size(), &written)) {
            throw CoreError(windows_error("解压文件数据失败"));
        }
        if (written != expected) {
            throw CoreError("解压后的数据长度与备份清单不一致。");
        }
        return output;
    }

private:
    Compression method_;
    CompressionHandle handle_ = nullptr;
};

/** 备份包外层固定头结构（116 字节）
 *  包含格式版本、压缩/加密方式、密钥派生参数、时间戳和负载校验值
 */
struct PackageHeader {
    std::uint32_t version = FORMAT_VERSION;  // 格式版本号（当前为 3）
    Compression compression = Compression::Stored;  // 压缩算法
    Encryption encryption = Encryption::None;          // 加密算法
    std::uint32_t kdf_iterations = DEFAULT_KDF_ITERATIONS;  // PBKDF2 迭代次数
    std::int64_t created_at = 0;      // 创建时间（Unix 时间戳）
    std::uint64_t plain_size = 0;     // 明文归档大小（字节）
    std::uint64_t payload_size = 0;   // 加密后负载大小（含认证标签）
    std::array<unsigned char, 16> salt{};   // PBKDF2 密钥派生盐值（随机生成）
    std::array<unsigned char, 12> nonce{};  // AEAD 随机数前缀（帧计数器附加在末尾）
    std::array<unsigned char, 32> payload_sha256{};  // 明文归档的 SHA-256 摘要
};

/** 将包头结构序列化为 116 字节的二进制数据
 *  用于写入备份包开头，以及作为 AEAD 的附加认证数据
 */
std::vector<unsigned char> serialize_header(const PackageHeader& header) {
    std::ostringstream out(std::ios::binary);
    write_bytes(out, PACKAGE_MAGIC.data(), PACKAGE_MAGIC.size());
    write_le<std::uint32_t>(out, header.version);
    write_le<std::uint32_t>(out, static_cast<std::uint32_t>(header.compression));
    write_le<std::uint32_t>(out, static_cast<std::uint32_t>(header.encryption));
    write_le<std::uint32_t>(out, header.kdf_iterations);
    write_le<std::int64_t>(out, header.created_at);
    write_le<std::uint64_t>(out, header.plain_size);
    write_le<std::uint64_t>(out, header.payload_size);
    write_bytes(out, header.salt.data(), header.salt.size());
    write_bytes(out, header.nonce.data(), header.nonce.size());
    write_bytes(out, header.payload_sha256.data(), header.payload_sha256.size());
    const std::string data = out.str();
    if (data.size() != PACKAGE_HEADER_SIZE) throw CoreError("内部包头长度错误。");
    return std::vector<unsigned char>(data.begin(), data.end());
}

/** 从输入流读取并反序列化备份包头
 *  包含魔数验证、版本检查和字段合法性校验
 */
PackageHeader read_header(std::istream& in) {
    std::array<unsigned char, 16> magic{};
    read_bytes(in, magic.data(), magic.size());
    if (magic != PACKAGE_MAGIC) {
        throw CoreError("不是 C++ v3 备份包，无法读取。");
    }
    PackageHeader header;
    header.version = read_le<std::uint32_t>(in);
    if (header.version != FORMAT_VERSION) throw CoreError("不支持的备份包版本。");
    const auto compression = read_le<std::uint32_t>(in);
    const auto encryption = read_le<std::uint32_t>(in);
    if (compression > static_cast<std::uint32_t>(Compression::Lzms)) throw CoreError("备份包压缩标识无效。");
    if (encryption > static_cast<std::uint32_t>(Encryption::ChaCha20Poly1305)) throw CoreError("备份包加密标识无效。");
    header.compression = static_cast<Compression>(compression);
    header.encryption = static_cast<Encryption>(encryption);
    header.kdf_iterations = read_le<std::uint32_t>(in);
    header.created_at = read_le<std::int64_t>(in);
    header.plain_size = read_le<std::uint64_t>(in);
    header.payload_size = read_le<std::uint64_t>(in);
    read_bytes(in, header.salt.data(), header.salt.size());
    read_bytes(in, header.nonce.data(), header.nonce.size());
    read_bytes(in, header.payload_sha256.data(), header.payload_sha256.size());
    if (header.plain_size > (1ull << 60) || header.payload_size > (1ull << 60)) {
        throw CoreError("备份包长度字段异常。");
    }
    return header;
}

/** 使用 PBKDF2-HMAC-SHA256 从密码派生 256 位加密密钥
 *  盐值和迭代次数存储在包头中，确保相同密码在不同包中产生不同密钥
 */
std::array<unsigned char, 32> derive_key(const std::string& password, const PackageHeader& header) {
    if (password.empty()) throw CoreError("备份包已加密，请输入密码。");
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    check_nt(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG), "打开 PBKDF2");
    std::array<unsigned char, 32> key{};
    const NTSTATUS status = BCryptDeriveKeyPBKDF2(
        algorithm,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        const_cast<PUCHAR>(header.salt.data()),
        static_cast<ULONG>(header.salt.size()),
        header.kdf_iterations,
        key.data(),
        static_cast<ULONG>(key.size()),
        0);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    check_nt(status, "派生加密密钥");
    return key;
}

/** AES-256-GCM 认证加密封装类
 *  基于 Windows CNG 实现，提供 encrypt/decrypt 操作
 *  每次加密/解密都需要唯一的 Nonce 和附加认证数据 (AAD)
 *  解密时自动验证 16 字节认证标签，失败则拒绝输出明文
 */
class AesGcm {
public:
    /** 构造函数：使用 256 位密钥初始化 AES-GCM 加密器 */
    explicit AesGcm(const std::array<unsigned char, 32>& key) {
        check_nt(BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_AES_ALGORITHM, nullptr, 0), "打开 AES");
        check_nt(BCryptSetProperty(
            algorithm_,
            BCRYPT_CHAINING_MODE,
            reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
            static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_GCM)),
            0), "设置 AES-GCM 模式");
        DWORD object_size = 0;
        DWORD result = 0;
        check_nt(BCryptGetProperty(algorithm_, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &result, 0), "读取 AES 参数");
        object_.resize(object_size);
        check_nt(BCryptGenerateSymmetricKey(
            algorithm_, &key_, object_.data(), static_cast<ULONG>(object_.size()),
            const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0), "创建 AES 密钥");
    }
    ~AesGcm() {
        if (key_) BCryptDestroyKey(key_);
        if (algorithm_) BCryptCloseAlgorithmProvider(algorithm_, 0);
        SecureZeroMemory(object_.data(), object_.size());
    }

    std::vector<unsigned char> encrypt(
        const std::vector<unsigned char>& plain,
        const std::array<unsigned char, 12>& nonce,
        const std::vector<unsigned char>& aad,
        std::array<unsigned char, 16>& tag) {
        std::vector<unsigned char> output(plain.size());
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce = const_cast<PUCHAR>(nonce.data());
        info.cbNonce = static_cast<ULONG>(nonce.size());
        info.pbAuthData = const_cast<PUCHAR>(aad.data());
        info.cbAuthData = static_cast<ULONG>(aad.size());
        info.pbTag = tag.data();
        info.cbTag = static_cast<ULONG>(tag.size());
        ULONG written = 0;
        check_nt(BCryptEncrypt(key_, const_cast<PUCHAR>(plain.data()), static_cast<ULONG>(plain.size()), &info,
                              nullptr, 0, output.data(), static_cast<ULONG>(output.size()), &written, 0), "加密备份数据");
        output.resize(written);
        return output;
    }

    std::vector<unsigned char> decrypt(
        const std::vector<unsigned char>& encrypted,
        const std::array<unsigned char, 12>& nonce,
        const std::vector<unsigned char>& aad,
        std::array<unsigned char, 16>& tag) {
        std::vector<unsigned char> output(encrypted.size());
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce = const_cast<PUCHAR>(nonce.data());
        info.cbNonce = static_cast<ULONG>(nonce.size());
        info.pbAuthData = const_cast<PUCHAR>(aad.data());
        info.cbAuthData = static_cast<ULONG>(aad.size());
        info.pbTag = tag.data();
        info.cbTag = static_cast<ULONG>(tag.size());
        ULONG written = 0;
        const NTSTATUS status = BCryptDecrypt(key_, const_cast<PUCHAR>(encrypted.data()), static_cast<ULONG>(encrypted.size()), &info,
                                              nullptr, 0, output.data(), static_cast<ULONG>(output.size()), &written, 0);
        if (status < 0) {
            throw CoreError("解密失败：密码错误或备份包已损坏。");
        }
        output.resize(written);
        return output;
    }

private:
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_KEY_HANDLE key_ = nullptr;
    std::vector<unsigned char> object_;
};

/** 从字节数组加载一个小端序 32 位无符号整数 */
std::uint32_t load_u32_le(const unsigned char* input) {
    return static_cast<std::uint32_t>(input[0]) |
           (static_cast<std::uint32_t>(input[1]) << 8) |
           (static_cast<std::uint32_t>(input[2]) << 16) |
           (static_cast<std::uint32_t>(input[3]) << 24);
}

/** 将 32 位无符号整数以小端序存入字节数组 */
void store_u32_le(unsigned char* output, std::uint32_t value) {
    output[0] = static_cast<unsigned char>(value & 0xff);
    output[1] = static_cast<unsigned char>((value >> 8) & 0xff);
    output[2] = static_cast<unsigned char>((value >> 16) & 0xff);
    output[3] = static_cast<unsigned char>((value >> 24) & 0xff);
}

/** 32 位循环左移（ChaCha20 核心运算） */
std::uint32_t rotate_left(std::uint32_t value, int count) {
    return (value << count) | (value >> (32 - count));
}

/** ChaCha20-Poly1305 认证加密类（纯 C++ 实现 RFC 8439）
 *  不依赖任何外部加密库，完全在 C++ 中实现：
 *  - ChaCha20 流密码：用于加密/解密数据
 *  - Poly1305 MAC：用于计算认证标签
 *  - AEAD 构造：组合以上两者，提供认证加密
 *  密钥在析构时通过 SecureZeroMemory 安全清零
 */
class ChaCha20Poly1305 {
public:
    /** 构造函数：保存 256 位密钥副本 */
    explicit ChaCha20Poly1305(const std::array<unsigned char, 32>& key) : key_(key) {}
    ~ChaCha20Poly1305() { SecureZeroMemory(key_.data(), key_.size()); }

    std::vector<unsigned char> encrypt(
        const std::vector<unsigned char>& plain,
        const std::array<unsigned char, 12>& nonce,
        const std::vector<unsigned char>& aad,
        std::array<unsigned char, 16>& tag) const {
        const auto first_block = chacha_block(0, nonce);
        std::array<unsigned char, 32> poly_key{};
        std::copy_n(first_block.begin(), poly_key.size(), poly_key.begin());
        auto encrypted = crypt(plain, nonce, 1);
        tag = poly1305_auth(build_mac_data(aad, encrypted), poly_key);
        return encrypted;
    }

    std::vector<unsigned char> decrypt(
        const std::vector<unsigned char>& encrypted,
        const std::array<unsigned char, 12>& nonce,
        const std::vector<unsigned char>& aad,
        const std::array<unsigned char, 16>& tag) const {
        const auto first_block = chacha_block(0, nonce);
        std::array<unsigned char, 32> poly_key{};
        std::copy_n(first_block.begin(), poly_key.size(), poly_key.begin());
        const auto expected_tag = poly1305_auth(build_mac_data(aad, encrypted), poly_key);
        unsigned char difference = 0;
        for (std::size_t i = 0; i < tag.size(); ++i) difference |= tag[i] ^ expected_tag[i];
        if (difference != 0) throw CoreError("解密失败：密码错误或备份包已损坏。");
        return crypt(encrypted, nonce, 1);
    }

private:
    static void quarter_round(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c, std::uint32_t& d) {
        a += b; d ^= a; d = rotate_left(d, 16);
        c += d; b ^= c; b = rotate_left(b, 12);
        a += b; d ^= a; d = rotate_left(d, 8);
        c += d; b ^= c; b = rotate_left(b, 7);
    }

    std::array<unsigned char, 64> chacha_block(std::uint32_t counter, const std::array<unsigned char, 12>& nonce) const {
        std::array<std::uint32_t, 16> state = {
            0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u,
            load_u32_le(key_.data()), load_u32_le(key_.data() + 4),
            load_u32_le(key_.data() + 8), load_u32_le(key_.data() + 12),
            load_u32_le(key_.data() + 16), load_u32_le(key_.data() + 20),
            load_u32_le(key_.data() + 24), load_u32_le(key_.data() + 28),
            counter, load_u32_le(nonce.data()), load_u32_le(nonce.data() + 4), load_u32_le(nonce.data() + 8)};
        auto working = state;
        for (int round = 0; round < 10; ++round) {
            quarter_round(working[0], working[4], working[8], working[12]);
            quarter_round(working[1], working[5], working[9], working[13]);
            quarter_round(working[2], working[6], working[10], working[14]);
            quarter_round(working[3], working[7], working[11], working[15]);
            quarter_round(working[0], working[5], working[10], working[15]);
            quarter_round(working[1], working[6], working[11], working[12]);
            quarter_round(working[2], working[7], working[8], working[13]);
            quarter_round(working[3], working[4], working[9], working[14]);
        }
        std::array<unsigned char, 64> output{};
        for (std::size_t i = 0; i < working.size(); ++i) {
            store_u32_le(output.data() + i * 4, working[i] + state[i]);
        }
        return output;
    }

    std::vector<unsigned char> crypt(
        const std::vector<unsigned char>& input,
        const std::array<unsigned char, 12>& nonce,
        std::uint32_t initial_counter) const {
        std::vector<unsigned char> output(input.size());
        std::uint32_t counter = initial_counter;
        std::size_t offset = 0;
        while (offset < input.size()) {
            if (counter == 0) throw CoreError("ChaCha20 数据帧超过计数器上限。");
            const auto stream = chacha_block(counter++, nonce);
            const std::size_t count = std::min<std::size_t>(stream.size(), input.size() - offset);
            for (std::size_t i = 0; i < count; ++i) output[offset + i] = input[offset + i] ^ stream[i];
            offset += count;
        }
        return output;
    }

    static void append_u64(std::vector<unsigned char>& output, std::uint64_t value) {
        for (int i = 0; i < 8; ++i) output.push_back(static_cast<unsigned char>((value >> (i * 8)) & 0xff));
    }

    static void append_padded(std::vector<unsigned char>& output, const std::vector<unsigned char>& value) {
        output.insert(output.end(), value.begin(), value.end());
        const std::size_t padding = (16 - value.size() % 16) % 16;
        output.insert(output.end(), padding, 0);
    }

    static std::vector<unsigned char> build_mac_data(
        const std::vector<unsigned char>& aad,
        const std::vector<unsigned char>& encrypted) {
        std::vector<unsigned char> data;
        data.reserve(aad.size() + encrypted.size() + 47);
        append_padded(data, aad);
        append_padded(data, encrypted);
        append_u64(data, aad.size());
        append_u64(data, encrypted.size());
        return data;
    }

    static std::array<unsigned char, 16> poly1305_auth(
        const std::vector<unsigned char>& message,
        const std::array<unsigned char, 32>& key) {
        const std::uint32_t t0 = load_u32_le(key.data());
        const std::uint32_t t1 = load_u32_le(key.data() + 4);
        const std::uint32_t t2 = load_u32_le(key.data() + 8);
        const std::uint32_t t3 = load_u32_le(key.data() + 12);
        const std::uint32_t r0 = t0 & 0x3ffffff;
        const std::uint32_t r1 = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03;
        const std::uint32_t r2 = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ff;
        const std::uint32_t r3 = ((t2 >> 14) | (t3 << 18)) & 0x3f03fff;
        const std::uint32_t r4 = (t3 >> 8) & 0x00fffff;
        const std::uint32_t s1 = r1 * 5;
        const std::uint32_t s2 = r2 * 5;
        const std::uint32_t s3 = r3 * 5;
        const std::uint32_t s4 = r4 * 5;
        std::uint32_t h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;

        auto process_block = [&](const unsigned char* block, std::uint32_t high_bit) {
            const std::uint32_t b0 = load_u32_le(block);
            const std::uint32_t b1 = load_u32_le(block + 4);
            const std::uint32_t b2 = load_u32_le(block + 8);
            const std::uint32_t b3 = load_u32_le(block + 12);
            h0 += b0 & 0x3ffffff;
            h1 += ((b0 >> 26) | (b1 << 6)) & 0x3ffffff;
            h2 += ((b1 >> 20) | (b2 << 12)) & 0x3ffffff;
            h3 += ((b2 >> 14) | (b3 << 18)) & 0x3ffffff;
            h4 += (b3 >> 8) | high_bit;

            std::uint64_t d0 = static_cast<std::uint64_t>(h0) * r0 + static_cast<std::uint64_t>(h1) * s4 +
                               static_cast<std::uint64_t>(h2) * s3 + static_cast<std::uint64_t>(h3) * s2 +
                               static_cast<std::uint64_t>(h4) * s1;
            std::uint64_t d1 = static_cast<std::uint64_t>(h0) * r1 + static_cast<std::uint64_t>(h1) * r0 +
                               static_cast<std::uint64_t>(h2) * s4 + static_cast<std::uint64_t>(h3) * s3 +
                               static_cast<std::uint64_t>(h4) * s2;
            std::uint64_t d2 = static_cast<std::uint64_t>(h0) * r2 + static_cast<std::uint64_t>(h1) * r1 +
                               static_cast<std::uint64_t>(h2) * r0 + static_cast<std::uint64_t>(h3) * s4 +
                               static_cast<std::uint64_t>(h4) * s3;
            std::uint64_t d3 = static_cast<std::uint64_t>(h0) * r3 + static_cast<std::uint64_t>(h1) * r2 +
                               static_cast<std::uint64_t>(h2) * r1 + static_cast<std::uint64_t>(h3) * r0 +
                               static_cast<std::uint64_t>(h4) * s4;
            std::uint64_t d4 = static_cast<std::uint64_t>(h0) * r4 + static_cast<std::uint64_t>(h1) * r3 +
                               static_cast<std::uint64_t>(h2) * r2 + static_cast<std::uint64_t>(h3) * r1 +
                               static_cast<std::uint64_t>(h4) * r0;

            std::uint32_t carry = static_cast<std::uint32_t>(d0 >> 26); h0 = static_cast<std::uint32_t>(d0) & 0x3ffffff; d1 += carry;
            carry = static_cast<std::uint32_t>(d1 >> 26); h1 = static_cast<std::uint32_t>(d1) & 0x3ffffff; d2 += carry;
            carry = static_cast<std::uint32_t>(d2 >> 26); h2 = static_cast<std::uint32_t>(d2) & 0x3ffffff; d3 += carry;
            carry = static_cast<std::uint32_t>(d3 >> 26); h3 = static_cast<std::uint32_t>(d3) & 0x3ffffff; d4 += carry;
            carry = static_cast<std::uint32_t>(d4 >> 26); h4 = static_cast<std::uint32_t>(d4) & 0x3ffffff;
            h0 += carry * 5;
            carry = h0 >> 26; h0 &= 0x3ffffff; h1 += carry;
        };

        std::size_t offset = 0;
        while (message.size() - offset >= 16) {
            process_block(message.data() + offset, 1u << 24);
            offset += 16;
        }
        if (offset < message.size()) {
            std::array<unsigned char, 16> final_block{};
            const std::size_t remaining = message.size() - offset;
            std::copy_n(message.data() + offset, remaining, final_block.begin());
            final_block[remaining] = 1;
            process_block(final_block.data(), 0);
        }

        std::uint32_t carry = h1 >> 26; h1 &= 0x3ffffff; h2 += carry;
        carry = h2 >> 26; h2 &= 0x3ffffff; h3 += carry;
        carry = h3 >> 26; h3 &= 0x3ffffff; h4 += carry;
        carry = h4 >> 26; h4 &= 0x3ffffff; h0 += carry * 5;
        carry = h0 >> 26; h0 &= 0x3ffffff; h1 += carry;

        std::uint32_t g0 = h0 + 5;
        carry = g0 >> 26; g0 &= 0x3ffffff;
        std::uint32_t g1 = h1 + carry; carry = g1 >> 26; g1 &= 0x3ffffff;
        std::uint32_t g2 = h2 + carry; carry = g2 >> 26; g2 &= 0x3ffffff;
        std::uint32_t g3 = h3 + carry; carry = g3 >> 26; g3 &= 0x3ffffff;
        std::uint32_t g4 = h4 + carry - (1u << 26);
        std::uint32_t mask = (g4 >> 31) - 1;
        g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
        mask = ~mask;
        h0 = (h0 & mask) | g0;
        h1 = (h1 & mask) | g1;
        h2 = (h2 & mask) | g2;
        h3 = (h3 & mask) | g3;
        h4 = (h4 & mask) | g4;

        // 每个组合字只保留低 32 位，再叠加一次性密钥和前一字的进位。
        // 若在移位前提升到 64 位，会把本应截断的高位重复计入下一字。
        std::uint64_t f0 = static_cast<std::uint32_t>(h0 | (h1 << 26)) + static_cast<std::uint64_t>(load_u32_le(key.data() + 16));
        std::uint64_t f1 = static_cast<std::uint32_t>((h1 >> 6) | (h2 << 20)) + static_cast<std::uint64_t>(load_u32_le(key.data() + 20)) + (f0 >> 32);
        std::uint64_t f2 = static_cast<std::uint32_t>((h2 >> 12) | (h3 << 14)) + static_cast<std::uint64_t>(load_u32_le(key.data() + 24)) + (f1 >> 32);
        std::uint64_t f3 = static_cast<std::uint32_t>((h3 >> 18) | (h4 << 8)) + static_cast<std::uint64_t>(load_u32_le(key.data() + 28)) + (f2 >> 32);
        std::array<unsigned char, 16> tag{};
        store_u32_le(tag.data(), static_cast<std::uint32_t>(f0));
        store_u32_le(tag.data() + 4, static_cast<std::uint32_t>(f1));
        store_u32_le(tag.data() + 8, static_cast<std::uint32_t>(f2));
        store_u32_le(tag.data() + 12, static_cast<std::uint32_t>(f3));
        return tag;
    }

    std::array<unsigned char, 32> key_{};
};

/** 为第 index 帧生成唯一 Nonce
 *  将帧序号编码到 Nonce 末尾 4 字节，确保每帧使用不同的随机数
 */
std::array<unsigned char, 12> frame_nonce(const PackageHeader& header, std::uint32_t index) {
    auto nonce = header.nonce;
    nonce[8] = static_cast<unsigned char>((index >> 24) & 0xff);
    nonce[9] = static_cast<unsigned char>((index >> 16) & 0xff);
    nonce[10] = static_cast<unsigned char>((index >> 8) & 0xff);
    nonce[11] = static_cast<unsigned char>(index & 0xff);
    return nonce;
}

/** 为第 index 帧构造附加认证数据 (AAD)
 *  AAD = 包头序列化 + 帧序号，参与 AEAD 认证但不加密
 *  篡改包头或帧顺序都会导致认证失败
 */
std::vector<unsigned char> frame_aad(const PackageHeader& header, std::uint32_t index) {
    auto aad = serialize_header(header);
    aad.push_back(static_cast<unsigned char>(index & 0xff));
    aad.push_back(static_cast<unsigned char>((index >> 8) & 0xff));
    aad.push_back(static_cast<unsigned char>((index >> 16) & 0xff));
    aad.push_back(static_cast<unsigned char>((index >> 24) & 0xff));
    return aad;
}

/** 获取当前时间的 Unix 时间戳（秒） */
std::int64_t unix_time_now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

/** 将 Unix 时间戳转换为 ISO 8601 格式字符串 (如 2026-07-16T02:00:00Z) */
std::string iso_time(std::int64_t unix_seconds) {
    std::time_t value = static_cast<std::time_t>(unix_seconds);
    std::tm tm{};
    gmtime_s(&tm, &value);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

/** 获取文件的最后修改时间（转换为 Unix 时间戳） */
std::int64_t path_mtime(const fs::path& path) {
    std::error_code ec;
    const auto file_time = fs::last_write_time(path, ec);
    if (ec) return 0;
    const auto system_time = std::chrono::time_point_cast<std::chrono::seconds>(
        file_time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return system_time.time_since_epoch().count();
}

/** 设置文件的最后修改时间（从 Unix 时间戳还原） */
void set_path_mtime(const fs::path& path, std::int64_t unix_seconds) {
    if (unix_seconds <= 0) return;
    const auto system_time = std::chrono::system_clock::time_point(std::chrono::seconds(unix_seconds));
    const auto file_time = fs::file_time_type::clock::now() + (system_time - std::chrono::system_clock::now());
    std::error_code ec;
    fs::last_write_time(path, file_time, ec);
}

/** 将字符串中的 ASCII 大写字母转为小写（用于大小写不敏感比较） */
std::string ascii_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
    });
    return value;
}

/** 通配符匹配算法
 *  支持 * (匹配任意字符序列) 和 ? (匹配单个字符)
 *  用于文件名筛选功能
 */
bool wildcard_match(const std::string& pattern, const std::string& text) {
    std::size_t p = 0, t = 0, star = std::string::npos, match = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p; ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            match = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++match;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

/** 文件筛选规则结构体
 *  用户在 GUI 中设置的所有筛选条件都收集在此结构中
 */
struct FilterRules {
    std::vector<std::string> includes;   // 包含关键词列表（路径必须包含其中之一）
    std::vector<std::string> excludes;   // 排除关键词列表（路径包含则跳过）
    std::string name_glob;               // 文件名通配符模式（如 *.docx）
    std::set<std::string> types;         // 允许的条目类型集合（file/dir/symlink）
    std::optional<std::uint64_t> min_size;    // 最小文件大小（字节）
    std::optional<std::uint64_t> max_size;    // 最大文件大小（字节）
    std::optional<std::int64_t> mtime_after;  // 修改时间下限（Unix 时间戳）
    std::optional<std::int64_t> mtime_before; // 修改时间上限（Unix 时间戳）
};

/** 归档条目类型枚举 */
enum class EntryType : std::uint8_t {
    Directory = 1,  // 目录条目（恢复时创建目录结构）
    File = 2,       // 普通文件（包含压缩数据和 SHA-256）
    Symlink = 3     // 符号链接（存储链接目标路径）
};

/** 扫描阶段生成的统一条目模型
 *  将文件系统的各种信息标准化为统一结构，供筛选和归档使用
 */
struct Entry {
    fs::path path;              // 文件在磁盘上的绝对路径
    std::string relative_path;  // 包内相对路径（UTF-8，用 / 分隔）
    EntryType type = EntryType::File;  // 条目类型
    std::uint64_t size = 0;     // 文件大小（目录和链接为 0）
    std::int64_t mtime = 0;     // 最后修改时间（Unix 时间戳）
    std::string link_target;    // 符号链接的目标路径（仅链接有效）
};

/** 将条目类型枚举转为字符串标识（用于筛选匹配） */
std::string entry_type_key(EntryType type) {
    switch (type) {
    case EntryType::Directory: return "dir";
    case EntryType::File: return "file";
    case EntryType::Symlink: return "symlink";
    }
    return "other";
}

/** 判断一个条目是否满足所有筛选规则
 *  所有条件取交集：必须同时满足包含、排除、通配、类型、大小和时间条件
 */
bool matches_filter(const Entry& entry, const FilterRules& rules) {
    const std::string lower_path = ascii_lower(entry.relative_path);
    if (!rules.includes.empty()) {
        bool matched = false;
        for (const auto& term : rules.includes) {
            if (lower_path.find(ascii_lower(term)) != std::string::npos) { matched = true; break; }
        }
        if (!matched) return false;
    }
    for (const auto& term : rules.excludes) {
        if (lower_path.find(ascii_lower(term)) != std::string::npos) return false;
    }
    if (!rules.name_glob.empty()) {
        const auto slash = entry.relative_path.find_last_of('/');
        const std::string name = slash == std::string::npos ? entry.relative_path : entry.relative_path.substr(slash + 1);
        if (!wildcard_match(ascii_lower(rules.name_glob), ascii_lower(name))) return false;
    }
    if (!rules.types.empty() && rules.types.count(entry_type_key(entry.type)) == 0) return false;
    if (rules.min_size && entry.size < *rules.min_size) return false;
    if (rules.max_size && entry.size > *rules.max_size) return false;
    if (rules.mtime_after && entry.mtime < *rules.mtime_after) return false;
    if (rules.mtime_before && entry.mtime > *rules.mtime_before) return false;
    return true;
}

/** 获取路径的规范化绝对形式，用于路径比较 */
fs::path normalized_absolute(const fs::path& path) {
    std::error_code ec;
    auto result = fs::absolute(path, ec).lexically_normal();
    return ec ? path.lexically_normal() : result;
}

/** 判断两个路径是否指向同一文件（不区分大小写，Windows 路径规则） */
bool same_windows_path(const fs::path& left, const fs::path& right) {
    return _wcsicmp(normalized_absolute(left).c_str(), normalized_absolute(right).c_str()) == 0;
}

/** 扫描备份源（文件或目录），生成符合筛选条件的条目列表
 *  - 单文件源：直接作为一个条目
 *  - 目录源：递归遍历所有子项，跳过输出文件自身
 *  遍历过程中会定期检查取消标志并输出扫描进度
 */
std::vector<Entry> scan_source(const fs::path& source, const fs::path& output, const FilterRules& rules) {
    std::vector<Entry> entries;
    std::error_code ec;
    const auto source_status = fs::symlink_status(source, ec);
    if (ec) {
        throw CoreError("无法读取备份源状态：" + utf8_from_wide(source.wstring()));
    }

    // 单独文件或符号链接作为备份源时，将其文件名直接作为包内相对路径。
    // 恢复时仍然写入用户选择的目标目录，不会携带源文件的绝对路径。
    if (!fs::is_directory(source_status)) {
        Entry entry;
        entry.path = source;
        entry.relative_path = utf8_from_wide(source.filename().generic_wstring());
        if (entry.relative_path.empty()) {
            throw CoreError("无法确定单文件备份源的文件名。");
        }
        entry.mtime = path_mtime(source);
        if (fs::is_symlink(source_status)) {
            entry.type = EntryType::Symlink;
            const auto target = fs::read_symlink(source, ec);
            if (!ec) entry.link_target = utf8_from_wide(target.generic_wstring());
            ec.clear();
        } else if (fs::is_regular_file(source_status)) {
            entry.type = EntryType::File;
            entry.size = fs::file_size(source, ec);
            if (ec) throw CoreError("无法读取源文件大小：" + entry.relative_path);
        } else {
            throw CoreError("备份源不是普通文件、目录或符号链接。");
        }
        if (matches_filter(entry, rules)) entries.push_back(entry);
        emit_progress("扫描", 1, 1, 0, 0, entry.relative_path, "正在扫描源文件");
        return entries;
    }

    fs::recursive_directory_iterator iterator(source, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    std::uint64_t scanned = 0;
    while (iterator != end) {
        check_cancelled();
        if (ec) {
            emit_log("warn", "扫描目录时跳过了一个不可访问的条目：" + ec.message());
            ec.clear();
            iterator.increment(ec);
            continue;
        }
        const fs::path path = iterator->path();
        if (same_windows_path(path, output)) {
            iterator.increment(ec);
            continue;
        }
        const auto status = iterator->symlink_status(ec);
        if (ec) {
            emit_log("warn", "无法读取条目状态，已跳过：" + utf8_from_wide(path.wstring()));
            ec.clear();
            iterator.increment(ec);
            continue;
        }
        Entry entry;
        entry.path = path;
        entry.relative_path = utf8_from_wide(path.lexically_relative(source).generic_wstring());
        entry.mtime = path_mtime(path);
        if (fs::is_symlink(status)) {
            entry.type = EntryType::Symlink;
            const auto target = fs::read_symlink(path, ec);
            if (!ec) entry.link_target = utf8_from_wide(target.generic_wstring());
            ec.clear();
            if (fs::is_directory(status)) iterator.disable_recursion_pending();
        } else if (fs::is_directory(status)) {
            entry.type = EntryType::Directory;
        } else if (fs::is_regular_file(status)) {
            entry.type = EntryType::File;
            entry.size = fs::file_size(path, ec);
            if (ec) {
                emit_log("warn", "无法读取文件大小，已跳过：" + entry.relative_path);
                ec.clear();
                iterator.increment(ec);
                continue;
            }
        } else {
            iterator.increment(ec);
            continue;
        }
        ++scanned;
        if (matches_filter(entry, rules)) entries.push_back(entry);
        emit_progress("扫描", scanned, scanned, 0, 0, entry.relative_path, "正在扫描源目录");
        iterator.increment(ec);
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
        return left.relative_path < right.relative_path;
    });
    return entries;
}

/** 归档内部条目头结构
 *  每个文件/目录/链接在归档中都有一个固定格式的头部
 */
struct ArchiveEntryHeader {
    EntryType type = EntryType::File;           // 条目类型
    Compression compression = Compression::Stored;  // 该条目使用的压缩方式
    std::uint32_t path_length = 0;   // 相对路径的 UTF-8 字节长度
    std::uint32_t link_length = 0;   // 链接目标路径的 UTF-8 字节长度
    std::uint64_t original_size = 0; // 文件原始大小（未压缩）
    std::int64_t mtime = 0;          // 文件修改时间（Unix 时间戳）
    std::uint64_t stored_size = 0;   // 压缩后存储大小（含帧头）
    std::array<unsigned char, 32> sha256{};  // 文件内容的 SHA-256 摘要
};

constexpr std::streamoff ENTRY_STORED_SIZE_OFFSET = 28;
constexpr std::streamoff ENTRY_SHA_OFFSET = 36;

/** 将条目头结构序列化写入归档流 */
void write_entry_header(std::ostream& out, const ArchiveEntryHeader& header) {
    out.put(static_cast<char>(header.type));
    out.put(static_cast<char>(header.compression));
    write_le<std::uint16_t>(out, 0);
    write_le<std::uint32_t>(out, header.path_length);
    write_le<std::uint32_t>(out, header.link_length);
    write_le<std::uint64_t>(out, header.original_size);
    write_le<std::int64_t>(out, header.mtime);
    write_le<std::uint64_t>(out, header.stored_size);
    write_bytes(out, header.sha256.data(), header.sha256.size());
}

/** 从归档流中反序列化读取一个条目头，包含类型和长度校验 */
ArchiveEntryHeader read_entry_header(std::istream& in) {
    ArchiveEntryHeader header;
    const int type = in.get();
    const int compression = in.get();
    if (type == EOF || compression == EOF) throw CoreError("备份归档意外结束。");
    if (type < 1 || type > 3) throw CoreError("备份归档条目类型无效。");
    if (compression < 0 || compression > 4) throw CoreError("备份归档压缩类型无效。");
    header.type = static_cast<EntryType>(type);
    header.compression = static_cast<Compression>(compression);
    (void)read_le<std::uint16_t>(in);
    header.path_length = read_le<std::uint32_t>(in);
    header.link_length = read_le<std::uint32_t>(in);
    header.original_size = read_le<std::uint64_t>(in);
    header.mtime = read_le<std::int64_t>(in);
    header.stored_size = read_le<std::uint64_t>(in);
    read_bytes(in, header.sha256.data(), header.sha256.size());
    if (header.path_length == 0 || header.path_length > 1024 * 1024 || header.link_length > 1024 * 1024) {
        throw CoreError("备份归档路径长度异常。");
    }
    return header;
}

/** 归档处理结果摘要 */
struct ArchiveSummary {
    std::uint64_t entries = 0;      // 总条目数（含目录和链接）
    std::uint64_t files = 0;        // 文件条目数
    std::uint64_t total_bytes = 0;  // 文件原始总字节数
};

/** 构建内部归档文件
 *  将所有条目按顺序写入临时归档：
 *  - 归档头：魔数 + 条目数 + 总字节数
 *  - 每个条目：条目头 + 路径 + 链接目标 + 压缩数据
 *  文件条目在写入过程中同时计算 SHA-256，写完后回填到条目头中
 */
ArchiveSummary build_archive(const fs::path& archive_path, const std::vector<Entry>& entries, Compression compression) {
    std::fstream out(archive_path, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!out) throw CoreError("无法创建临时归档文件。");
    write_bytes(out, ARCHIVE_MAGIC.data(), ARCHIVE_MAGIC.size());
    write_le<std::uint64_t>(out, entries.size());
    std::uint64_t total_bytes = 0;
    for (const auto& entry : entries) if (entry.type == EntryType::File) total_bytes += entry.size;
    write_le<std::uint64_t>(out, total_bytes);

    Compressor compressor(compression);
    std::vector<unsigned char> input(IO_CHUNK);
    std::uint64_t done_bytes = 0;
    std::uint64_t files = 0;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        check_cancelled();
        const Entry& entry = entries[index];
        ArchiveEntryHeader header;
        header.type = entry.type;
        header.compression = entry.type == EntryType::File ? compression : Compression::Stored;
        header.path_length = static_cast<std::uint32_t>(entry.relative_path.size());
        header.link_length = static_cast<std::uint32_t>(entry.link_target.size());
        header.original_size = entry.size;
        header.mtime = entry.mtime;

        const std::streampos header_position = out.tellp();
        write_entry_header(out, header);
        write_bytes(out, entry.relative_path.data(), entry.relative_path.size());
        write_bytes(out, entry.link_target.data(), entry.link_target.size());
        const std::streampos data_position = out.tellp();

        if (entry.type == EntryType::File) {
            ++files;
            std::ifstream source_file(entry.path, std::ios::binary);
            if (!source_file) throw CoreError("无法读取源文件：" + entry.relative_path);
            Hash256 hash;
            std::uint64_t file_read = 0;
            while (source_file) {
                check_cancelled();
                source_file.read(reinterpret_cast<char*>(input.data()), static_cast<std::streamsize>(input.size()));
                const std::streamsize count = source_file.gcount();
                if (count <= 0) break;
                const std::size_t size = static_cast<std::size_t>(count);
                hash.update(input.data(), size);
                file_read += size;
                done_bytes += size;
                if (compression == Compression::Stored) {
                    write_bytes(out, input.data(), size);
                } else {
                    std::vector<unsigned char> chunk(input.begin(), input.begin() + static_cast<std::ptrdiff_t>(size));
                    const auto compressed = compressor.compress(chunk);
                    write_le<std::uint32_t>(out, static_cast<std::uint32_t>(size));
                    write_le<std::uint32_t>(out, static_cast<std::uint32_t>(compressed.size()));
                    write_bytes(out, compressed.data(), compressed.size());
                }
                emit_progress("写入", entries.size(), index, total_bytes, done_bytes, entry.relative_path, "正在压缩文件");
            }
            if (file_read != entry.size) throw CoreError("源文件在备份期间发生变化：" + entry.relative_path);
            header.sha256 = hash.finish();
        }

        const std::streampos end_position = out.tellp();
        header.stored_size = static_cast<std::uint64_t>(end_position - data_position);
        out.seekp(header_position + ENTRY_STORED_SIZE_OFFSET);
        write_le<std::uint64_t>(out, header.stored_size);
        out.seekp(header_position + ENTRY_SHA_OFFSET);
        write_bytes(out, header.sha256.data(), header.sha256.size());
        out.seekp(end_position);
        emit_progress("写入", entries.size(), index + 1, total_bytes, done_bytes, entry.relative_path, "条目写入完成");
    }
    out.flush();
    if (!out) throw CoreError("刷新临时归档失败。");
    emit_log("info", "C++ 归档负载生成完成。");
    return {entries.size(), files, total_bytes};
}

/** 计算文件的 SHA-256 哈希值，同时统计文件大小
 *  按 1MB 分块读取，避免大文件整体载入内存
 */
std::array<unsigned char, 32> hash_file(const fs::path& path, std::uint64_t& size) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw CoreError("无法读取临时归档。");
    Hash256 hash;
    std::vector<unsigned char> buffer(IO_CHUNK);
    size = 0;
    while (in) {
        check_cancelled();
        in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto count = in.gcount();
        if (count <= 0) break;
        hash.update(buffer.data(), static_cast<std::size_t>(count));
        size += static_cast<std::uint64_t>(count);
    }
    return hash.finish();
}

/** 将临时归档封装为最终备份包
 *  流程：
 *  1. 计算归档 SHA-256 并填入包头
 *  2. 若启用加密：生成随机盐值和 Nonce → PBKDF2 派生密钥 → 分帧 AEAD 加密
 *  3. 若不加密：直接复制归档数据
 *  4. 写入临时文件后原子替换目标文件
 */
void wrap_archive(
    const fs::path& archive,
    const fs::path& output,
    Compression compression,
    Encryption encryption,
    const std::string& password,
    std::uint32_t iterations) {
    PackageHeader header;
    header.compression = compression;
    header.encryption = encryption;
    header.kdf_iterations = iterations;
    header.created_at = unix_time_now();
    header.payload_sha256 = hash_file(archive, header.plain_size);
    if (encryption != Encryption::None) {
        check_nt(BCryptGenRandom(nullptr, header.salt.data(), static_cast<ULONG>(header.salt.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG), "生成加密盐值");
        check_nt(BCryptGenRandom(nullptr, header.nonce.data(), 8, BCRYPT_USE_SYSTEM_PREFERRED_RNG), "生成加密随机数");
        const std::uint64_t frame_count = (header.plain_size + IO_CHUNK - 1) / IO_CHUNK;
        if (frame_count > 0xffffffffull) throw CoreError("备份包过大，超过认证加密分帧上限。");
        header.payload_size = header.plain_size + frame_count * 20;
    } else {
        header.payload_size = header.plain_size;
    }

    const fs::path temp_path = sibling_temp(output);
    TempPath temp(temp_path);
    std::ifstream in(archive, std::ios::binary);
    std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
    if (!in || !out) throw CoreError("无法创建最终备份包。");
    const auto serialized = serialize_header(header);
    write_bytes(out, serialized.data(), serialized.size());

    std::vector<unsigned char> buffer(IO_CHUNK);
    std::uint64_t processed = 0;
    if (encryption == Encryption::None) {
        while (in) {
            check_cancelled();
            in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            const auto count = in.gcount();
            if (count <= 0) break;
            write_bytes(out, buffer.data(), static_cast<std::size_t>(count));
            processed += static_cast<std::uint64_t>(count);
            emit_progress("封装", 0, 0, header.plain_size, processed, "", "正在封装备份包");
        }
    } else {
        const auto key = derive_key(password, header);
        std::unique_ptr<AesGcm> aes;
        std::unique_ptr<ChaCha20Poly1305> chacha;
        if (encryption == Encryption::Aes256Gcm) aes = std::make_unique<AesGcm>(key);
        else if (encryption == Encryption::ChaCha20Poly1305) chacha = std::make_unique<ChaCha20Poly1305>(key);
        else throw CoreError("未知认证加密方式。");
        std::uint32_t index = 0;
        while (in) {
            check_cancelled();
            in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            const auto count = in.gcount();
            if (count <= 0) break;
            std::vector<unsigned char> plain(buffer.begin(), buffer.begin() + count);
            std::array<unsigned char, 16> tag{};
            const auto nonce = frame_nonce(header, index);
            const auto aad = frame_aad(header, index);
            const auto encrypted = aes ? aes->encrypt(plain, nonce, aad, tag) : chacha->encrypt(plain, nonce, aad, tag);
            write_le<std::uint32_t>(out, static_cast<std::uint32_t>(plain.size()));
            write_bytes(out, tag.data(), tag.size());
            write_bytes(out, encrypted.data(), encrypted.size());
            processed += static_cast<std::uint64_t>(plain.size());
            ++index;
            const std::string method_name = encryption == Encryption::Aes256Gcm ? "AES-256-GCM" : "ChaCha20-Poly1305";
            emit_progress("加密", 0, 0, header.plain_size, processed, "", "正在使用 " + method_name + " 加密");
        }
    }
    out.flush();
    out.close();
    if (processed != header.plain_size) throw CoreError("临时归档大小在封装期间发生变化。");
    atomic_replace(temp_path, output);
    temp.release();
}

/** 打开备份包后的返回结构
 *  包含解密后的包头信息和临时归档文件路径
 *  temp 使用 RAII 确保临时文件在使用完毕后自动删除
 */
struct OpenedPackage {
    PackageHeader header;             // 解析后的包头
    fs::path archive_path;            // 解密后的临时归档文件路径
    std::unique_ptr<TempPath> temp;   // 临时文件 RAII 管理器
};

/** 打开并解密备份包，生成临时归档文件
 *  流程：
 *  1. 读取并校验包头
 *  2. 若加密：派生密钥 → 逐帧 AEAD 解密（每帧自动验证认证标签）
 *  3. 若不加密：直接复制负载
 *  4. 验证总长度和归档 SHA-256
 *  返回：包头信息 + 解密后的临时归档路径
 */
OpenedPackage unwrap_package(const fs::path& package, const std::string& password) {
    std::ifstream in(package, std::ios::binary);
    if (!in) throw CoreError("备份包不存在或无法读取。");
    PackageHeader header = read_header(in);
    const fs::path archive_path = system_temp(L"pbackup_cpp_archive_");
    auto temp = std::make_unique<TempPath>(archive_path);
    std::ofstream out(archive_path, std::ios::binary | std::ios::trunc);
    if (!out) throw CoreError("无法创建临时解包文件。");

    Hash256 hash;
    std::uint64_t plain_written = 0;
    std::uint64_t payload_read = 0;
    std::vector<unsigned char> buffer(IO_CHUNK);
    if (header.encryption == Encryption::None) {
        while (payload_read < header.payload_size) {
            check_cancelled();
            const std::size_t wanted = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), header.payload_size - payload_read));
            read_bytes(in, buffer.data(), wanted);
            write_bytes(out, buffer.data(), wanted);
            hash.update(buffer.data(), wanted);
            payload_read += wanted;
            plain_written += wanted;
        }
    } else {
        const auto key = derive_key(password, header);
        std::unique_ptr<AesGcm> aes;
        std::unique_ptr<ChaCha20Poly1305> chacha;
        if (header.encryption == Encryption::Aes256Gcm) aes = std::make_unique<AesGcm>(key);
        else if (header.encryption == Encryption::ChaCha20Poly1305) chacha = std::make_unique<ChaCha20Poly1305>(key);
        else throw CoreError("未知认证加密方式。");
        std::uint32_t index = 0;
        while (payload_read < header.payload_size) {
            check_cancelled();
            if (header.payload_size - payload_read < 20) throw CoreError("加密负载帧已截断。");
            const std::uint32_t plain_size = read_le<std::uint32_t>(in);
            payload_read += 4;
            if (plain_size == 0 || plain_size > IO_CHUNK || payload_read + 16ull + plain_size > header.payload_size) {
                throw CoreError("加密负载帧长度异常。");
            }
            std::array<unsigned char, 16> tag{};
            read_bytes(in, tag.data(), tag.size());
            payload_read += tag.size();
            std::vector<unsigned char> encrypted(plain_size);
            read_bytes(in, encrypted.data(), encrypted.size());
            payload_read += encrypted.size();
            const auto nonce = frame_nonce(header, index);
            const auto aad = frame_aad(header, index);
            const auto plain = aes ? aes->decrypt(encrypted, nonce, aad, tag) : chacha->decrypt(encrypted, nonce, aad, tag);
            write_bytes(out, plain.data(), plain.size());
            hash.update(plain.data(), plain.size());
            plain_written += plain.size();
            ++index;
            emit_progress("解密", 0, 0, header.plain_size, plain_written, "", "正在解密备份包");
        }
    }
    out.flush();
    out.close();
    if (plain_written != header.plain_size || payload_read != header.payload_size) throw CoreError("备份包长度校验失败。");
    if (in.peek() != EOF) throw CoreError("备份包尾部包含异常附加数据。");
    if (!secure_equal(hash.finish(), header.payload_sha256)) throw CoreError("备份包负载 SHA-256 校验失败。");
    return {header, archive_path, std::move(temp)};
}

/** 从流中读取指定长度的 UTF-8 字符串并验证编码有效性 */
std::string read_utf8_string(std::istream& in, std::uint32_t length) {
    std::string value(length, '\0');
    read_bytes(in, value.data(), value.size());
    (void)wide_from_utf8(value);
    return value;
}

/** 安全路径构造：防止目录穿越攻击
 *  拒绝绝对路径、空路径、..、驱动器路径等恶意路径
 *  确保恢复的文件只能写入目标目录内部
 */
fs::path safe_destination(const fs::path& root, const std::string& relative) {
    if (relative.empty() || relative.front() == '/' || relative.front() == '\\') {
        throw CoreError("备份包包含不安全路径：" + relative);
    }
    std::string normalized = relative;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::istringstream parts(normalized);
    std::string part;
    fs::path result = normalized_absolute(root);
    while (std::getline(parts, part, '/')) {
        if (part.empty() || part == "." || part == ".." || part.find(':') != std::string::npos) {
            throw CoreError("备份包包含不安全路径：" + relative);
        }
        result /= wide_from_utf8(part);
    }
    return result.lexically_normal();
}

/** 检查目标路径的所有父目录是否包含符号链接
 *  防止通过符号链接将文件写出目标目录（符号链接穿越攻击）
 */
void ensure_no_symlink_parent(const fs::path& root, const fs::path& target) {
    fs::path current = normalized_absolute(root);
    const fs::path relative = target.lexically_relative(current);
    for (auto iterator = relative.begin(); iterator != relative.end(); ++iterator) {
        auto next = iterator;
        ++next;
        if (next == relative.end()) break;
        current /= *iterator;
        std::error_code ec;
        const auto status = fs::symlink_status(current, ec);
        if (!ec && fs::is_symlink(status)) {
            throw CoreError("恢复路径经过符号链接，已拒绝写入：" + utf8_from_wide(current.wstring()));
        }
    }
}

/** 读取并处理一个文件条目的压缩数据
 *  - 解压每个数据块并计算 SHA-256
 *  - 若 destination 非空则同时写出到目标文件
 *  - 最终验证还原长度和哈希值一致性
 */
void consume_file_data(
    std::istream& archive,
    const ArchiveEntryHeader& header,
    std::ostream* destination,
    const std::string& relative_path) {
    Hash256 hash;
    std::uint64_t stored_read = 0;
    std::uint64_t plain_written = 0;
    if (header.compression == Compression::Stored) {
        if (header.stored_size != header.original_size) throw CoreError("无压缩条目长度不一致：" + relative_path);
        std::vector<unsigned char> buffer(IO_CHUNK);
        while (stored_read < header.stored_size) {
            check_cancelled();
            const std::size_t wanted = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), header.stored_size - stored_read));
            read_bytes(archive, buffer.data(), wanted);
            if (destination) write_bytes(*destination, buffer.data(), wanted);
            hash.update(buffer.data(), wanted);
            stored_read += wanted;
            plain_written += wanted;
        }
    } else {
        Decompressor decompressor(header.compression);
        while (stored_read < header.stored_size) {
            check_cancelled();
            if (header.stored_size - stored_read < 8) throw CoreError("压缩条目帧已截断：" + relative_path);
            const std::uint32_t raw_size = read_le<std::uint32_t>(archive);
            const std::uint32_t compressed_size = read_le<std::uint32_t>(archive);
            stored_read += 8;
            if (raw_size == 0 || raw_size > IO_CHUNK || compressed_size == 0 ||
                stored_read + compressed_size > header.stored_size || plain_written + raw_size > header.original_size) {
                throw CoreError("压缩条目帧长度异常：" + relative_path);
            }
            std::vector<unsigned char> compressed(compressed_size);
            read_bytes(archive, compressed.data(), compressed.size());
            stored_read += compressed.size();
            const auto plain = decompressor.decompress(compressed, raw_size);
            if (destination) write_bytes(*destination, plain.data(), plain.size());
            hash.update(plain.data(), plain.size());
            plain_written += plain.size();
        }
    }
    if (plain_written != header.original_size || !secure_equal(hash.finish(), header.sha256)) {
        throw CoreError("文件内容 SHA-256 校验失败：" + relative_path);
    }
}

/** 处理归档内容：恢复文件或仅校验
 *  - destination 有值：恢复模式，逐条目创建目录/文件/链接
 *  - destination 为空：校验模式，只验证结构和 SHA-256 不写出文件
 *  每个文件恢复前都通过 safe_destination 和 ensure_no_symlink_parent 防止路径攻击
 *  文件先写入临时文件，SHA-256 正确后再原子替换目标
 */
ArchiveSummary process_archive(const fs::path& archive_path, const std::optional<fs::path>& destination, bool overwrite) {
    std::ifstream archive(archive_path, std::ios::binary);
    if (!archive) throw CoreError("无法读取解包后的归档。");
    std::array<unsigned char, 8> magic{};
    read_bytes(archive, magic.data(), magic.size());
    if (magic != ARCHIVE_MAGIC) throw CoreError("内部归档魔数错误。");
    const std::uint64_t entry_count = read_le<std::uint64_t>(archive);
    const std::uint64_t total_bytes = read_le<std::uint64_t>(archive);
    if (entry_count > 10000000ull || total_bytes > (1ull << 60)) throw CoreError("内部归档清单长度异常。");
    std::uint64_t files = 0;
    std::uint64_t done_bytes = 0;
    for (std::uint64_t index = 0; index < entry_count; ++index) {
        check_cancelled();
        const auto header = read_entry_header(archive);
        const std::string relative = read_utf8_string(archive, header.path_length);
        const std::string link_target = read_utf8_string(archive, header.link_length);
        emit_progress(destination ? "恢复" : "校验", entry_count, index, total_bytes, done_bytes, relative, destination ? "正在恢复" : "正在校验");

        if (!destination) {
            if (header.type == EntryType::File) {
                consume_file_data(archive, header, nullptr, relative);
                ++files;
                done_bytes += header.original_size;
            } else if (header.stored_size != 0) {
                throw CoreError("非文件条目包含异常数据：" + relative);
            }
        } else {
            const fs::path target = safe_destination(*destination, relative);
            ensure_no_symlink_parent(*destination, target);
            std::error_code ec;
            if (header.type == EntryType::Directory) {
                fs::create_directories(target, ec);
                if (ec) throw CoreError("无法创建目录：" + relative);
                if (header.stored_size != 0) throw CoreError("目录条目包含异常数据：" + relative);
                set_path_mtime(target, header.mtime);
            } else if (header.type == EntryType::Symlink) {
                if (header.stored_size != 0) throw CoreError("符号链接条目包含异常数据：" + relative);
                if (fs::exists(target, ec) || fs::is_symlink(fs::symlink_status(target, ec))) {
                    if (!overwrite) throw CoreError("目标条目已存在，未启用覆盖：" + relative);
                    fs::remove_all(target, ec);
                    if (ec) throw CoreError("无法覆盖目标条目：" + relative);
                }
                fs::create_directories(target.parent_path(), ec);
                ec.clear();
                try {
                    fs::create_symlink(fs::path(wide_from_utf8(link_target)), target);
                } catch (const std::exception& exc) {
                    const fs::path marker = target.wstring() + L".symlink.txt";
                    std::ofstream marker_out(marker, std::ios::binary | std::ios::trunc);
                    const std::string text = "原符号链接目标：" + link_target + "\r\n创建失败原因：" + exc.what() + "\r\n";
                    write_bytes(marker_out, text.data(), text.size());
                    emit_log("warn", "无法创建符号链接，已写入说明文件：" + utf8_from_wide(marker.wstring()));
                }
            } else {
                ++files;
                if (fs::exists(target, ec)) {
                    if (!overwrite) throw CoreError("目标文件已存在，未启用覆盖：" + relative);
                    if (fs::is_directory(target, ec)) throw CoreError("目标位置是目录，无法覆盖为文件：" + relative);
                }
                fs::create_directories(target.parent_path(), ec);
                if (ec) throw CoreError("无法创建目标目录：" + relative);
                const fs::path temp_file = sibling_temp(target);
                TempPath temp(temp_file);
                std::ofstream out(temp_file, std::ios::binary | std::ios::trunc);
                if (!out) throw CoreError("无法创建恢复文件：" + relative);
                consume_file_data(archive, header, &out, relative);
                out.flush();
                out.close();
                atomic_replace(temp_file, target);
                temp.release();
                set_path_mtime(target, header.mtime);
                done_bytes += header.original_size;
            }
        }
        emit_progress(destination ? "恢复" : "校验", entry_count, index + 1, total_bytes, done_bytes, relative, destination ? "条目恢复完成" : "条目校验完成");
    }
    if (archive.peek() != EOF) throw CoreError("内部归档尾部包含异常附加数据。");
    return {entry_count, files, total_bytes};
}

/** 命令行参数解析器
 *  将 --key value 格式的参数对存入 map
 *  支持重复参数（如多个 --include）通过 all() 获取
 */
class Arguments {
public:
    /** 构造函数：从 argv[start] 开始解析所有 --key value 对 */
    Arguments(int argc, wchar_t** argv, int start) {
        for (int i = start; i < argc; ++i) {
            std::wstring key = argv[i];
            if (key.rfind(L"--", 0) != 0) throw CoreError("无效参数：" + utf8_from_wide(key));
            if (i + 1 >= argc) throw CoreError("参数缺少值：" + utf8_from_wide(key));
            values_[key.substr(2)].push_back(argv[++i]);
        }
    }

    std::wstring require(const std::wstring& key) const {
        const auto it = values_.find(key);
        if (it == values_.end() || it->second.empty()) throw CoreError("缺少参数：--" + utf8_from_wide(key));
        return it->second.back();
    }

    std::wstring get(const std::wstring& key, const std::wstring& fallback = L"") const {
        const auto it = values_.find(key);
        return it == values_.end() || it->second.empty() ? fallback : it->second.back();
    }

    std::vector<std::wstring> all(const std::wstring& key) const {
        const auto it = values_.find(key);
        return it == values_.end() ? std::vector<std::wstring>{} : it->second;
    }

private:
    std::map<std::wstring, std::vector<std::wstring>> values_;
};

/** 将宽字符串解析为无符号 64 位整数，失败时抛出带参数名的错误 */
std::uint64_t parse_u64(const std::wstring& value, const std::string& name) {
    try {
        std::size_t used = 0;
        const auto result = std::stoull(value, &used);
        if (used != value.size()) throw std::invalid_argument("tail");
        return result;
    } catch (...) {
        throw CoreError(name + "不是有效整数。");
    }
}

/** 将宽字符串解析为有符号 64 位整数，失败时抛出带参数名的错误 */
std::int64_t parse_i64(const std::wstring& value, const std::string& name) {
    try {
        std::size_t used = 0;
        const auto result = std::stoll(value, &used);
        if (used != value.size()) throw std::invalid_argument("tail");
        return result;
    } catch (...) {
        throw CoreError(name + "不是有效整数。");
    }
}

/** 从环境变量读取密码（默认为 PBACKUP_PASSWORD）
 *  密码通过环境变量传递而非命令行参数，避免在进程列表中暴露
 */
std::string password_from_environment(const Arguments& args) {
    const std::wstring variable = args.get(L"password-env", L"PBACKUP_PASSWORD");
    const DWORD needed = GetEnvironmentVariableW(variable.c_str(), nullptr, 0);
    if (needed == 0) return {};
    std::wstring value(needed, L'\0');
    const DWORD written = GetEnvironmentVariableW(variable.c_str(), value.data(), needed);
    if (written == 0) return {};
    value.resize(written);
    return utf8_from_wide(value);
}

/** 输出任务结果事件，GUI 据此判断任务成功并显示结果摘要 */
void emit_result(const std::string& object_json) {
    emit_json("{\"type\":\"result\",\"data\":" + object_json + "}");
}

/** capabilities 命令：报告后端支持的压缩和加密算法列表
 *  GUI 启动时调用此命令，根据返回值动态填充下拉框选项
 */
void command_capabilities() {
    std::ostringstream compression;
    compression << "\"stored\":\"无压缩（最快）\"";
    if (CompressionApi::instance().available()) {
        compression << ",\"mszip\":\"MSZIP / Deflate（通用）\""
                    << ",\"xpress\":\"XPRESS（快速）\""
                    << ",\"xpress_huff\":\"XPRESS Huffman（平衡）\""
                    << ",\"lzms\":\"LZMS（高压缩率）\"";
    }
    emit_result(
        "{\"format\":\"PBACKUP-CPP3\","
        "\"compression\":{" + compression.str() + "},"
        "\"encryption\":{"
        "\"none\":\"不加密\","
        "\"aes256_gcm\":\"AES-256-GCM（认证加密）\","
        "\"chacha20_poly1305\":\"ChaCha20-Poly1305（认证加密）\"}}"
    );
}

/** backup 命令：执行完整的备份流程
 *  流程：参数校验 → 目录扫描 → 筛选 → 归档构建 → 加密封装 → 原子写入
 */
void command_backup(const Arguments& args) {
    const fs::path source(args.require(L"source"));
    const fs::path output(args.require(L"output"));
    const std::wstring default_compression = CompressionApi::instance().available() ? L"mszip" : L"stored";
    const Compression compression = parse_compression(args.get(L"compression", default_compression));
    const Encryption encryption = parse_encryption(args.get(L"encryption", L"none"));
    const std::string password = password_from_environment(args);
    require_compression_support(compression);
    const auto iterations_value = parse_u64(args.get(L"kdf", std::to_wstring(DEFAULT_KDF_ITERATIONS)), "KDF 迭代次数");
    if (iterations_value < 100000 || iterations_value > 10000000) throw CoreError("KDF 迭代次数超出允许范围。");
    if (encryption != Encryption::None && password.empty()) throw CoreError("选择加密后必须填写密码。");
    std::error_code source_error;
    const auto source_status = fs::symlink_status(source, source_error);
    if (source_error || (!fs::is_directory(source_status) && !fs::is_regular_file(source_status) && !fs::is_symlink(source_status))) {
        throw CoreError("备份源不存在，或不是普通文件、文件夹或符号链接。");
    }
    if (same_windows_path(source, output)) {
        throw CoreError("备份包输出位置不能与备份源文件相同。");
    }

    FilterRules rules;
    for (const auto& value : args.all(L"include")) rules.includes.push_back(utf8_from_wide(value));
    for (const auto& value : args.all(L"exclude")) rules.excludes.push_back(utf8_from_wide(value));
    rules.name_glob = utf8_from_wide(args.get(L"glob"));
    for (const auto& value : args.all(L"type")) rules.types.insert(utf8_from_wide(value));
    if (!args.get(L"min-size").empty()) rules.min_size = parse_u64(args.get(L"min-size"), "最小大小");
    if (!args.get(L"max-size").empty()) rules.max_size = parse_u64(args.get(L"max-size"), "最大大小");
    if (!args.get(L"mtime-after").empty()) rules.mtime_after = parse_i64(args.get(L"mtime-after"), "起始时间");
    if (!args.get(L"mtime-before").empty()) rules.mtime_before = parse_i64(args.get(L"mtime-before"), "结束时间");

    emit_log("info", fs::is_directory(source_status) ? "开始由 C++ 核心扫描源目录。" : "开始由 C++ 核心读取源文件。");
    const auto entries = scan_source(source, output, rules);
    emit_log("info", "扫描完成，符合条件的条目数：" + std::to_string(entries.size()));
    const fs::path archive_path = system_temp(L"pbackup_cpp_build_");
    TempPath archive(archive_path);
    const auto summary = build_archive(archive_path, entries, compression);
    wrap_archive(archive_path, output, compression, encryption, password, static_cast<std::uint32_t>(iterations_value));
    emit_progress("完成", summary.entries, summary.entries, std::max<std::uint64_t>(summary.total_bytes, 1),
                  std::max<std::uint64_t>(summary.total_bytes, 1), "", "备份完成");
    emit_log("ok", "备份包已写入：" + utf8_from_wide(output.wstring()));
    std::ostringstream result;
    result << "{\"entries\":" << summary.entries
           << ",\"files\":" << summary.files
           << ",\"total_bytes\":" << summary.total_bytes
           << ",\"compression\":" << quote(compression_key(compression))
           << ",\"encryption\":" << quote(encryption_key(encryption))
           << ",\"output\":" << quote(utf8_from_wide(output.wstring())) << "}";
    emit_result(result.str());
}

/** restore 命令：执行完整的恢复流程
 *  流程：解密 → 归档校验 → 逐条目恢复（目录/文件/链接）→ SHA-256 验证
 */
void command_restore(const Arguments& args) {
    const fs::path package(args.require(L"package"));
    const fs::path destination(args.require(L"destination"));
    const bool overwrite = args.get(L"overwrite", L"0") == L"1";
    std::error_code ec;
    fs::create_directories(destination, ec);
    if (ec) throw CoreError("无法创建恢复目标目录。");
    auto opened = unwrap_package(package, password_from_environment(args));
    const auto summary = process_archive(opened.archive_path, destination, overwrite);
    emit_progress("完成", summary.entries, summary.entries, std::max<std::uint64_t>(summary.total_bytes, 1),
                  std::max<std::uint64_t>(summary.total_bytes, 1), "", "恢复完成");
    emit_log("ok", "恢复完成：" + utf8_from_wide(destination.wstring()));
    std::ostringstream result;
    result << "{\"entries\":" << summary.entries
           << ",\"files\":" << summary.files
           << ",\"destination\":" << quote(utf8_from_wide(destination.wstring()))
           << ",\"compression\":" << quote(compression_key(opened.header.compression))
           << ",\"encryption\":" << quote(encryption_key(opened.header.encryption)) << "}";
    emit_result(result.str());
}

/** verify 命令：只读校验备份包完整性
 *  与 restore 类似但不写出任何文件，仅验证 AEAD 标签和所有 SHA-256
 */
void command_verify(const Arguments& args) {
    const fs::path package(args.require(L"package"));
    auto opened = unwrap_package(package, password_from_environment(args));
    const auto summary = process_archive(opened.archive_path, std::nullopt, false);
    std::ostringstream result;
    result << "{\"entries\":" << summary.entries
           << ",\"files\":" << summary.files
           << ",\"total_bytes\":" << summary.total_bytes
           << ",\"compression\":" << quote(compression_key(opened.header.compression))
           << ",\"encryption\":" << quote(encryption_key(opened.header.encryption))
           << ",\"created_at\":" << quote(iso_time(opened.header.created_at)) << "}";
    emit_result(result.str());
}

/** header 命令：读取并显示备份包头信息（无需密码）
 *  用于查看备份包的压缩方式、加密方式、创建时间等元数据
 */
void command_header(const Arguments& args) {
    const fs::path package(args.require(L"package"));
    std::ifstream in(package, std::ios::binary);
    if (!in) throw CoreError("备份包不存在或无法读取。");
    const auto header = read_header(in);
    std::ostringstream result;
    result << "{\"format\":\"PBACKUP-CPP3\",\"version\":" << header.version
           << ",\"created_at\":" << quote(iso_time(header.created_at))
           << ",\"compression\":" << quote(compression_key(header.compression))
           << ",\"encryption\":" << quote(encryption_key(header.encryption))
           << ",\"kdf_iterations\":" << header.kdf_iterations
           << ",\"plain_size\":" << header.plain_size
           << ",\"payload_size\":" << header.payload_size << "}";
    emit_result(result.str());
}

/**
 * @brief 备份验证比对命令 (compare)
 *
 * 将备份包中记录的文件哈希与源目录中当前文件的哈希进行比对，
 * 验证备份数据是否与源数据一致。这是对 verify 命令的增强：
 * - verify: 仅校验备份包内部完整性（SHA-256、AEAD 标签）
 * - compare: 额外对比源文件是否与备份时一致（检测源文件变更）
 *
 * 输出结果包含：匹配文件数、不匹配文件数、源中新增文件数、源中缺失文件数
 */
void command_compare(const Arguments& args) {
    /* 1. 打开并解密备份包 */
    const fs::path package(args.require(L"package"));
    const fs::path source(args.require(L"source"));
    auto opened = unwrap_package(package, password_from_environment(args));

    /* 2. 解析归档，提取每个文件的相对路径和 SHA-256 */
    std::ifstream archive(opened.archive_path, std::ios::binary);
    if (!archive) throw CoreError("无法读取解包后的归档。");
    std::array<unsigned char, 8> magic{};
    read_bytes(archive, magic.data(), magic.size());
    if (magic != ARCHIVE_MAGIC) throw CoreError("内部归档魔数错误。");
    const std::uint64_t entry_count = read_le<std::uint64_t>(archive);
    const std::uint64_t total_bytes = read_le<std::uint64_t>(archive);
    (void)total_bytes;

    /* 收集备份包中所有文件条目的路径 -> SHA-256 映射 */
    std::map<std::string, std::array<unsigned char, 32>> backup_hashes;
    std::uint64_t backup_files = 0;

    for (std::uint64_t i = 0; i < entry_count; ++i) {
        check_cancelled();
        const auto header = read_entry_header(archive);
        const std::string relative = read_utf8_string(archive, header.path_length);
        (void)read_utf8_string(archive, header.link_length);  /* 跳过链接目标 */

        if (header.type == EntryType::File) {
            /* 校验文件数据完整性（内部 SHA-256 验证） */
            consume_file_data(archive, header, nullptr, relative);
            backup_hashes[relative] = header.sha256;
            ++backup_files;
        } else {
            /* 跳过非文件条目的存储数据 */
            if (header.stored_size > 0) {
                std::vector<unsigned char> skip_buf(IO_CHUNK);
                std::uint64_t remaining = header.stored_size;
                while (remaining > 0) {
                    const std::size_t chunk = static_cast<std::size_t>(
                        std::min<std::uint64_t>(skip_buf.size(), remaining));
                    read_bytes(archive, skip_buf.data(), chunk);
                    remaining -= chunk;
                }
            }
        }
        emit_progress("比对", entry_count, i + 1, 0, 0, relative, "正在读取备份包内容");
    }

    /* 3. 扫描源目录，计算每个文件的当前 SHA-256 并与备份比对 */
    std::uint64_t matched = 0;      /* 哈希一致的文件数 */
    std::uint64_t mismatched = 0;   /* 哈希不一致的文件数 */
    std::uint64_t source_only = 0;  /* 源中有但备份中没有的文件数 */
    std::vector<std::string> mismatch_list;  /* 不匹配的文件路径列表 */
    std::set<std::string> checked_paths;     /* 已检查的路径集合 */

    std::error_code ec;
    const auto source_status = fs::symlink_status(source, ec);
    if (ec || !fs::exists(source_status)) {
        throw CoreError("备份源目录不存在：" + utf8_from_wide(source.wstring()));
    }

    /* 处理单文件备份源 */
    if (!fs::is_directory(source_status)) {
        const std::string filename = utf8_from_wide(source.filename().generic_wstring());
        auto it = backup_hashes.find(filename);
        if (it != backup_hashes.end()) {
            /* 计算当前源文件的 SHA-256 */
            std::uint64_t file_size = 0;
            const auto current_hash = hash_file(source, file_size);
            if (secure_equal(current_hash, it->second)) {
                ++matched;
                emit_log("info", "文件一致：" + filename);
            } else {
                ++mismatched;
                mismatch_list.push_back(filename);
                emit_log("warn", "文件不一致：" + filename);
            }
            checked_paths.insert(filename);
        } else {
            ++source_only;
            emit_log("warn", "源文件不在备份中：" + filename);
        }
    } else {
        /* 递归扫描源目录 */
        fs::recursive_directory_iterator dir_iter(source, fs::directory_options::skip_permission_denied, ec);
        fs::recursive_directory_iterator dir_end;
        std::uint64_t scan_count = 0;

        while (dir_iter != dir_end) {
            check_cancelled();
            if (ec) { ec.clear(); dir_iter.increment(ec); continue; }

            const fs::path file_path = dir_iter->path();
            const auto file_status = dir_iter->symlink_status(ec);
            if (ec || !fs::is_regular_file(file_status)) {
                ec.clear();
                dir_iter.increment(ec);
                continue;
            }

            const std::string relative = utf8_from_wide(
                file_path.lexically_relative(source).generic_wstring());
            ++scan_count;

            auto it = backup_hashes.find(relative);
            if (it != backup_hashes.end()) {
                /* 计算当前文件的 SHA-256 并与备份记录比对 */
                std::uint64_t file_size = 0;
                const auto current_hash = hash_file(file_path, file_size);
                if (secure_equal(current_hash, it->second)) {
                    ++matched;
                } else {
                    ++mismatched;
                    mismatch_list.push_back(relative);
                    emit_log("warn", "文件内容已变更：" + relative);
                }
                checked_paths.insert(relative);
            } else {
                ++source_only;
            }

            emit_progress("比对", scan_count, scan_count, 0, 0, relative, "正在比对源文件");
            dir_iter.increment(ec);
        }
    }

    /* 4. 检查备份中有但源中已不存在的文件 */
    std::uint64_t backup_only = 0;
    for (const auto& [path, _] : backup_hashes) {
        if (checked_paths.count(path) == 0) {
            ++backup_only;
            emit_log("warn", "备份中的文件在源中已不存在：" + path);
        }
    }

    /* 5. 输出比对结果 */
    const bool all_match = (mismatched == 0 && backup_only == 0);
    emit_log(all_match ? "ok" : "warn",
             all_match ? "备份验证通过：所有文件与源一致。"
                       : "备份验证发现差异，详见结果。");

    std::ostringstream result;
    result << "{\"matched\":" << matched
           << ",\"mismatched\":" << mismatched
           << ",\"source_only\":" << source_only
           << ",\"backup_only\":" << backup_only
           << ",\"backup_files\":" << backup_files
           << ",\"all_match\":" << (all_match ? "true" : "false")
           << ",\"compression\":" << quote(compression_key(opened.header.compression))
           << ",\"encryption\":" << quote(encryption_key(opened.header.encryption));

    /* 附加不匹配文件列表（最多前 20 个） */
    if (!mismatch_list.empty()) {
        result << ",\"mismatched_files\":[";
        for (std::size_t i = 0; i < std::min<std::size_t>(mismatch_list.size(), 20); ++i) {
            if (i > 0) result << ",";
            result << quote(mismatch_list[i]);
        }
        result << "]";
    }
    result << "}";
    emit_result(result.str());
}

} // namespace (匿名命名空间结束)

/* ═══════════════════════════════════════════════════════════════════════════
 * DLL 导出函数实现
 *
 * 以下函数使用 extern "C" 链接，是 pbackup_core.dll 的公共 API。
 * 主程序 (pbackup_main.cpp) 通过 LoadLibrary + GetProcAddress 调用这些函数。
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 执行备份核心命令
 *
 * 接收命令行参数数组，argv[0] 为命令名（如 "backup"、"restore" 等），
 * 后续为 --key value 格式的参数对。
 *
 * @param argc 参数个数
 * @param argv 宽字符参数数组
 * @return 0=成功, 2=业务错误, 3=文件系统错误, 4=其他异常
 */
extern "C" PBACKUP_API int pbackup_execute(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::ios::sync_with_stdio(false);
    try {
        if (argc < 1) throw CoreError("缺少命令。可用命令：capabilities、backup、restore、verify、compare、header。");
        const std::wstring command = argv[0];
        const Arguments args(argc, argv, 1);
        if (command == L"capabilities") command_capabilities();
        else if (command == L"backup") command_backup(args);
        else if (command == L"restore") command_restore(args);
        else if (command == L"verify") command_verify(args);
        else if (command == L"compare") command_compare(args);
        else if (command == L"header") command_header(args);
        else throw CoreError("未知命令：" + utf8_from_wide(command));
        return 0;
    } catch (const CoreError& exc) {
        emit_json("{\"type\":\"error\",\"message\":" + quote(exc.what()) + "}");
        return 2;
    } catch (const fs::filesystem_error& exc) {
        emit_json("{\"type\":\"error\",\"message\":" + quote(std::string("文件系统错误：") + exc.what()) + "}");
        return 3;
    } catch (const std::exception& exc) {
        emit_json("{\"type\":\"error\",\"message\":" + quote(std::string("C++ 核心异常：") + exc.what()) + "}");
        return 4;
    }
}

/**
 * @brief 请求取消当前操作
 *
 * 设置全局取消标志，核心逻辑在下一个检查点会抛出取消异常。
 * 线程安全：使用 std::atomic_bool 保证多线程可见性。
 */
extern "C" PBACKUP_API void pbackup_cancel(void) {
    g_cancelled.store(true);
}

/**
 * @brief 获取库版本号
 * @return 静态字符串 "0.7.2"
 */
extern "C" PBACKUP_API const char* pbackup_version(void) {
    return "0.7.2";
}
