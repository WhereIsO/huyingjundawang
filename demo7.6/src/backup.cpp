#include "backup.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sdb {
namespace {

constexpr std::array<char, 8> kMagic = {'S', 'D', 'B', 'K', '2', '0', '2', '6'};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kArchiveFlagEncrypted = 1u << 0;
constexpr std::uint32_t kEntryFlagCompressed = 1u << 0;
constexpr std::uint32_t kEntryFlagEncrypted = 1u << 1;
constexpr std::uint8_t kEntryDirectory = 1;
constexpr std::uint8_t kEntryFile = 2;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;
constexpr std::uint64_t kWindowsUnixEpochDiff = 116444736000000000ull;
constexpr std::uint64_t kFiletimeTicksPerSecond = 10000000ull;

struct PendingEntry {
    std::string absolutePath;
    std::string relativePath;
    std::uint8_t type = 0;
    DWORD attributes = 0;
    std::uint64_t size = 0;
    std::int64_t modifiedTime = 0;
};

struct EntryMeta {
    std::uint8_t type = 0;
    std::uint32_t flags = 0;
    std::string path;
    std::uint64_t originalSize = 0;
    std::uint64_t storedSize = 0;
    std::int64_t modifiedTime = 0;
    std::uint32_t permissions = 0;
    std::uint64_t checksum = 0;
};

struct ArchiveHeader {
    std::uint32_t version = 0;
    std::uint32_t flags = 0;
    std::uint64_t createdAt = 0;
    std::uint64_t passwordFingerprint = 0;
    std::uint64_t entryCount = 0;
};

bool isSeparator(char ch) {
    return ch == '\\' || ch == '/';
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string toNativePath(std::string value) {
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

std::string toArchivePath(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

bool isDriveRoot(const std::string& path) {
    return path.size() == 3 && std::isalpha(static_cast<unsigned char>(path[0])) &&
           path[1] == ':' && isSeparator(path[2]);
}

std::string trimTrailingSeparators(std::string path) {
    while (path.size() > 1 && isSeparator(path.back()) && !isDriveRoot(path)) {
        path.pop_back();
    }
    return path;
}

std::string absolutePath(const std::string& value) {
    DWORD needed = GetFullPathNameA(value.c_str(), 0, nullptr, nullptr);
    if (needed == 0) {
        throw std::runtime_error("invalid path: " + value);
    }
    std::string buffer(needed, '\0');
    DWORD written = GetFullPathNameA(value.c_str(), needed, &buffer[0], nullptr);
    if (written == 0 || written >= needed) {
        throw std::runtime_error("failed to normalize path: " + value);
    }
    buffer.resize(written);
    return trimTrailingSeparators(toNativePath(buffer));
}

std::string parentPath(const std::string& value) {
    std::string path = trimTrailingSeparators(toNativePath(value));
    std::size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return "";
    }
    if (pos == 2 && path.size() >= 3 && path[1] == ':') {
        return path.substr(0, 3);
    }
    return path.substr(0, pos);
}

std::string filenameOf(const std::string& value) {
    std::string path = trimTrailingSeparators(value);
    std::size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string extensionOf(const std::string& value) {
    std::string name = filenameOf(value);
    std::size_t pos = name.find_last_of('.');
    if (pos == std::string::npos || pos == 0) {
        return "";
    }
    return name.substr(pos);
}

std::string joinPath(const std::string& base, const std::string& child) {
    if (base.empty()) {
        return toNativePath(child);
    }
    if (child.empty()) {
        return base;
    }
    std::string nativeChild = toNativePath(child);
    if (isSeparator(base.back())) {
        return base + nativeChild;
    }
    return base + "\\" + nativeChild;
}

bool pathExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool isDirectoryPath(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool createDirectories(const std::string& input) {
    std::string path = trimTrailingSeparators(toNativePath(input));
    if (path.empty() || isDirectoryPath(path)) {
        return true;
    }

    std::size_t start = 0;
    if (path.size() >= 3 && path[1] == ':' && isSeparator(path[2])) {
        start = 3;
    } else if (path.size() >= 2 && isSeparator(path[0]) && isSeparator(path[1])) {
        std::size_t first = path.find('\\', 2);
        std::size_t second = first == std::string::npos ? std::string::npos : path.find('\\', first + 1);
        start = second == std::string::npos ? path.size() : second + 1;
    }

    for (std::size_t i = start; i <= path.size(); ++i) {
        if (i != path.size() && !isSeparator(path[i])) {
            continue;
        }
        std::string part = path.substr(0, i);
        if (part.empty() || isDirectoryPath(part)) {
            continue;
        }
        if (!CreateDirectoryA(part.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }
    return isDirectoryPath(path);
}

bool isPathInside(const std::string& candidate, const std::string& root) {
    std::string child = toLower(trimTrailingSeparators(absolutePath(candidate)));
    std::string base = toLower(trimTrailingSeparators(absolutePath(root)));
    if (child == base) {
        return true;
    }
    if (!isSeparator(base.back())) {
        base.push_back('\\');
    }
    return child.rfind(base, 0) == 0;
}

std::uint64_t fileSizeFromFindData(const WIN32_FIND_DATAA& data) {
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) |
           static_cast<std::uint64_t>(data.nFileSizeLow);
}

std::int64_t fileTimeToUnix(const FILETIME& value) {
    ULARGE_INTEGER raw{};
    raw.LowPart = value.dwLowDateTime;
    raw.HighPart = value.dwHighDateTime;
    if (raw.QuadPart < kWindowsUnixEpochDiff) {
        return 0;
    }
    return static_cast<std::int64_t>((raw.QuadPart - kWindowsUnixEpochDiff) /
                                    kFiletimeTicksPerSecond);
}

FILETIME unixToFileTime(std::int64_t value) {
    ULARGE_INTEGER raw{};
    raw.QuadPart = static_cast<std::uint64_t>(value) * kFiletimeTicksPerSecond +
                   kWindowsUnixEpochDiff;
    FILETIME result{};
    result.dwLowDateTime = raw.LowPart;
    result.dwHighDateTime = raw.HighPart;
    return result;
}

void applyMetadata(const std::string& path, DWORD attributes, std::int64_t modifiedTime) {
    SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (isDirectoryPath(path)) {
        flags |= FILE_FLAG_BACKUP_SEMANTICS;
    }
    HANDLE handle = CreateFileA(path.c_str(),
                                FILE_WRITE_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr,
                                OPEN_EXISTING,
                                flags,
                                nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        FILETIME writeTime = unixToFileTime(modifiedTime);
        SetFileTime(handle, nullptr, nullptr, &writeTime);
        CloseHandle(handle);
    }

    DWORD allowed = attributes &
                    (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                     FILE_ATTRIBUTE_ARCHIVE);
    if (allowed == 0) {
        allowed = FILE_ATTRIBUTE_NORMAL;
    }
    SetFileAttributesA(path.c_str(), allowed);
}

std::string normalizeExtension(std::string value) {
    value = toLower(value);
    if (!value.empty() && value.front() != '.') {
        value.insert(value.begin(), '.');
    }
    return value;
}

bool containsValue(const std::vector<std::string>& values, const std::string& candidate) {
    return std::find(values.begin(), values.end(), candidate) != values.end();
}

std::uint64_t fnv1aBytes(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t hash = kFnvOffset;
    for (std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= kFnvPrime;
    }
    return hash;
}

std::uint64_t fnv1aString(const std::string& value) {
    std::uint64_t hash = kFnvOffset;
    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= kFnvPrime;
    }
    return hash;
}

std::uint64_t passwordFingerprint(const std::string& password) {
    if (password.empty()) {
        return 0;
    }
    return fnv1aString("sdb-password:" + password);
}

std::uint64_t nextSplitMix64(std::uint64_t& state) {
    state += 0x9e3779b97f4a7c15ull;
    std::uint64_t value = state;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

void cryptBuffer(std::vector<std::uint8_t>& data,
                 const std::string& password,
                 const std::string& path,
                 std::uint64_t originalSize) {
    std::uint64_t state = fnv1aString("sdb-stream:" + password);
    state ^= fnv1aString(path) + 0x9e3779b97f4a7c15ull + (state << 6) + (state >> 2);
    state ^= originalSize + 0xbf58476d1ce4e5b9ull;

    std::uint64_t block = 0;
    int remaining = 0;
    for (std::uint8_t& byte : data) {
        if (remaining == 0) {
            block = nextSplitMix64(state);
            remaining = 8;
        }
        byte ^= static_cast<std::uint8_t>(block & 0xffu);
        block >>= 8;
        --remaining;
    }
}

std::vector<std::uint8_t> rleCompress(const std::vector<std::uint8_t>& input) {
    std::vector<std::uint8_t> output;
    output.reserve(input.size());
    std::size_t i = 0;
    while (i < input.size()) {
        std::uint8_t value = input[i];
        std::uint8_t count = 1;
        while (i + count < input.size() && input[i + count] == value && count < 255) {
            ++count;
        }
        output.push_back(count);
        output.push_back(value);
        i += count;
    }
    return output;
}

std::vector<std::uint8_t> rleDecompress(const std::vector<std::uint8_t>& input,
                                        std::uint64_t expectedSize) {
    if (input.size() % 2 != 0) {
        throw std::runtime_error("corrupt archive: invalid RLE payload");
    }
    if (expectedSize > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("file is too large for this platform");
    }

    std::vector<std::uint8_t> output;
    output.reserve(static_cast<std::size_t>(expectedSize));
    for (std::size_t i = 0; i < input.size(); i += 2) {
        std::uint8_t count = input[i];
        std::uint8_t value = input[i + 1];
        if (output.size() + count > expectedSize) {
            throw std::runtime_error("corrupt archive: decompressed size overflow");
        }
        output.insert(output.end(), count, value);
    }
    if (output.size() != expectedSize) {
        throw std::runtime_error("corrupt archive: decompressed size mismatch");
    }
    return output;
}

void writeU8(std::ostream& out, std::uint8_t value) {
    out.put(static_cast<char>(value));
    if (!out) {
        throw std::runtime_error("failed to write archive");
    }
}

void writeU32(std::ostream& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out.put(static_cast<char>((value >> (8 * i)) & 0xffu));
    }
    if (!out) {
        throw std::runtime_error("failed to write archive");
    }
}

void writeU64(std::ostream& out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.put(static_cast<char>((value >> (8 * i)) & 0xffu));
    }
    if (!out) {
        throw std::runtime_error("failed to write archive");
    }
}

void writeI64(std::ostream& out, std::int64_t value) {
    writeU64(out, static_cast<std::uint64_t>(value));
}

std::uint8_t readU8(std::istream& in) {
    int ch = in.get();
    if (ch == EOF) {
        throw std::runtime_error("corrupt archive: unexpected end of file");
    }
    return static_cast<std::uint8_t>(ch);
}

std::uint32_t readU32(std::istream& in) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(readU8(in)) << (8 * i);
    }
    return value;
}

std::uint64_t readU64(std::istream& in) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<std::uint64_t>(readU8(in)) << (8 * i);
    }
    return value;
}

std::int64_t readI64(std::istream& in) {
    return static_cast<std::int64_t>(readU64(in));
}

void writeString(std::ostream& out, const std::string& value) {
    writeU64(out, static_cast<std::uint64_t>(value.size()));
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
    if (!out) {
        throw std::runtime_error("failed to write archive");
    }
}

std::string readString(std::istream& in) {
    std::uint64_t size = readU64(in);
    if (size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("corrupt archive: string is too large");
    }
    std::string value(static_cast<std::size_t>(size), '\0');
    if (!value.empty()) {
        in.read(&value[0], static_cast<std::streamsize>(value.size()));
    }
    if (!in) {
        throw std::runtime_error("corrupt archive: unexpected end of file");
    }
    return value;
}

std::vector<std::uint8_t> readExactBytes(std::istream& in, std::uint64_t size) {
    if (size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("file is too large for this platform");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) {
        in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!in) {
        throw std::runtime_error("corrupt archive: unexpected end of file");
    }
    return bytes;
}

std::vector<std::uint8_t> readFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open file for reading: " + path);
    }
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    if (size < 0) {
        throw std::runtime_error("failed to determine file size: " + path);
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) {
        in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!in) {
        throw std::runtime_error("failed to read file: " + path);
    }
    return bytes;
}

void writeFileBytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    if (pathExists(path)) {
        SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to open file for writing: " + path);
    }
    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    if (!out) {
        throw std::runtime_error("failed to write file: " + path);
    }
}

bool isSafeArchivePath(const std::string& value) {
    if (value.empty() || value.size() > 4096 || value.find(':') != std::string::npos) {
        return false;
    }
    if (isSeparator(value.front()) || isSeparator(value.back())) {
        return false;
    }

    std::string normalized = toArchivePath(value);
    std::stringstream parts(normalized);
    std::string part;
    while (std::getline(parts, part, '/')) {
        if (part.empty() || part == "." || part == "..") {
            return false;
        }
    }
    return true;
}

bool matchesFilter(const PendingEntry& entry, const FilterOptions& filter) {
    if (entry.size < filter.minSize || entry.size > filter.maxSize) {
        return false;
    }

    std::string extension = normalizeExtension(extensionOf(entry.absolutePath));
    if (!filter.includeExtensions.empty() && !containsValue(filter.includeExtensions, extension)) {
        return false;
    }
    if (!filter.excludeExtensions.empty() && containsValue(filter.excludeExtensions, extension)) {
        return false;
    }

    if (!filter.nameContains.empty()) {
        std::string filename = toLower(filenameOf(entry.absolutePath));
        std::string needle = toLower(filter.nameContains);
        if (filename.find(needle) == std::string::npos) {
            return false;
        }
    }

    if (filter.hasModifiedAfter && entry.modifiedTime <= filter.modifiedAfter) {
        return false;
    }
    return true;
}

std::string relativeArchivePath(const std::string& absolute, const std::string& root) {
    std::string rel = absolute.substr(root.size());
    while (!rel.empty() && isSeparator(rel.front())) {
        rel.erase(rel.begin());
    }
    return toArchivePath(rel);
}

void collectEntriesRecursive(const std::string& root,
                             const std::string& current,
                             const FilterOptions& filter,
                             std::vector<PendingEntry>& entries) {
    std::string pattern = joinPath(current, "*");
    WIN32_FIND_DATAA data{};
    HANDLE handle = FindFirstFileA(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        std::string name = data.cFileName;
        if (name == "." || name == "..") {
            continue;
        }

        DWORD attrs = data.dwFileAttributes;
        if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }

        std::string fullPath = joinPath(current, name);
        PendingEntry entry;
        entry.absolutePath = fullPath;
        entry.relativePath = relativeArchivePath(fullPath, root);
        entry.attributes = attrs;
        entry.size = fileSizeFromFindData(data);
        entry.modifiedTime = fileTimeToUnix(data.ftLastWriteTime);

        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            entry.type = kEntryDirectory;
            entries.push_back(entry);
            collectEntriesRecursive(root, fullPath, filter, entries);
        } else {
            entry.type = kEntryFile;
            if (matchesFilter(entry, filter)) {
                entries.push_back(entry);
            }
        }
    } while (FindNextFileA(handle, &data));

    FindClose(handle);
}

std::vector<PendingEntry> collectEntries(const std::string& source, const FilterOptions& filter) {
    std::vector<PendingEntry> entries;
    collectEntriesRecursive(source, source, filter, entries);
    std::sort(entries.begin(), entries.end(), [](const PendingEntry& lhs, const PendingEntry& rhs) {
        if (lhs.relativePath == rhs.relativePath) {
            return lhs.type < rhs.type;
        }
        return lhs.relativePath < rhs.relativePath;
    });
    return entries;
}

void writeHeader(std::ostream& out, const ArchiveHeader& header) {
    out.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
    writeU32(out, header.version);
    writeU32(out, header.flags);
    writeU64(out, header.createdAt);
    writeU64(out, header.passwordFingerprint);
    writeU64(out, header.entryCount);
}

ArchiveHeader readHeader(std::istream& in) {
    std::array<char, 8> magic{};
    in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!in || magic != kMagic) {
        throw std::runtime_error("invalid archive format");
    }

    ArchiveHeader header;
    header.version = readU32(in);
    header.flags = readU32(in);
    header.createdAt = readU64(in);
    header.passwordFingerprint = readU64(in);
    header.entryCount = readU64(in);
    if (header.version != kVersion) {
        throw std::runtime_error("unsupported archive version");
    }
    return header;
}

void writeEntryMeta(std::ostream& out, const EntryMeta& meta) {
    writeU8(out, meta.type);
    writeU32(out, meta.flags);
    writeString(out, meta.path);
    writeU64(out, meta.originalSize);
    writeU64(out, meta.storedSize);
    writeI64(out, meta.modifiedTime);
    writeU32(out, meta.permissions);
    writeU64(out, meta.checksum);
}

EntryMeta readEntryMeta(std::istream& in) {
    EntryMeta meta;
    meta.type = readU8(in);
    meta.flags = readU32(in);
    meta.path = readString(in);
    meta.originalSize = readU64(in);
    meta.storedSize = readU64(in);
    meta.modifiedTime = readI64(in);
    meta.permissions = readU32(in);
    meta.checksum = readU64(in);
    if (meta.type != kEntryDirectory && meta.type != kEntryFile) {
        throw std::runtime_error("corrupt archive: unknown entry type");
    }
    if (!isSafeArchivePath(meta.path)) {
        throw std::runtime_error("corrupt archive: unsafe entry path");
    }
    return meta;
}

std::vector<std::uint8_t> decodePayload(const EntryMeta& meta,
                                        std::vector<std::uint8_t> payload,
                                        const std::string& password) {
    if ((meta.flags & kEntryFlagEncrypted) != 0) {
        cryptBuffer(payload, password, meta.path, meta.originalSize);
    }
    if ((meta.flags & kEntryFlagCompressed) != 0) {
        payload = rleDecompress(payload, meta.originalSize);
    }
    if (payload.size() != meta.originalSize) {
        throw std::runtime_error("corrupt archive: restored size mismatch for " + meta.path);
    }
    if (fnv1aBytes(payload) != meta.checksum) {
        throw std::runtime_error("corrupt archive or wrong password: checksum mismatch for " +
                                 meta.path);
    }
    return payload;
}

std::string entryTypeName(std::uint8_t type) {
    return type == kEntryDirectory ? "directory" : "file";
}

void validatePassword(const ArchiveHeader& header, const std::string& password) {
    if ((header.flags & kArchiveFlagEncrypted) == 0) {
        return;
    }
    if (password.empty()) {
        throw std::runtime_error("archive is encrypted; provide --password");
    }
    if (passwordFingerprint(password) != header.passwordFingerprint) {
        throw std::runtime_error("wrong password");
    }
}

}  // namespace

std::vector<std::string> splitCommaList(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item.erase(item.begin(), std::find_if(item.begin(), item.end(), [](unsigned char ch) {
                       return !std::isspace(ch);
                   }));
        item.erase(std::find_if(item.rbegin(), item.rend(), [](unsigned char ch) {
                       return !std::isspace(ch);
                   }).base(),
                   item.end());
        if (!item.empty()) {
            result.push_back(normalizeExtension(item));
        }
    }
    return result;
}

bool parseDate(const std::string& value, std::int64_t& timestamp) {
    std::tm timeValue{};
    std::istringstream stream(value);
    stream >> std::get_time(&timeValue, "%Y-%m-%d");
    if (stream.fail()) {
        return false;
    }
    timeValue.tm_hour = 0;
    timeValue.tm_min = 0;
    timeValue.tm_sec = 0;
    timeValue.tm_isdst = -1;
    std::time_t converted = std::mktime(&timeValue);
    if (converted == static_cast<std::time_t>(-1)) {
        return false;
    }
    timestamp = static_cast<std::int64_t>(converted);
    return true;
}

OperationStats createBackup(const std::string& sourceDir,
                            const std::string& archiveFile,
                            const BackupOptions& options) {
    std::string source = absolutePath(sourceDir);
    if (!isDirectoryPath(source)) {
        throw std::runtime_error("source must be an existing directory: " + sourceDir);
    }

    std::string archiveAbsolute = absolutePath(archiveFile);
    std::string archiveParent = parentPath(archiveAbsolute);
    if (!archiveParent.empty() && !createDirectories(archiveParent)) {
        throw std::runtime_error("failed to create archive directory: " + archiveParent);
    }
    if (isPathInside(archiveAbsolute, source)) {
        throw std::runtime_error("archive file must not be placed inside the source directory");
    }

    std::vector<PendingEntry> entries = collectEntries(source, options.filter);
    ArchiveHeader header;
    header.version = kVersion;
    header.flags = options.password.empty() ? 0 : kArchiveFlagEncrypted;
    header.createdAt = static_cast<std::uint64_t>(std::time(nullptr));
    header.passwordFingerprint = passwordFingerprint(options.password);
    header.entryCount = static_cast<std::uint64_t>(entries.size());

    std::ofstream out(archiveAbsolute, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to create archive: " + archiveAbsolute);
    }
    writeHeader(out, header);

    OperationStats stats;
    for (const PendingEntry& entry : entries) {
        EntryMeta meta;
        meta.type = entry.type;
        meta.path = entry.relativePath;
        meta.modifiedTime = entry.modifiedTime;
        meta.permissions = entry.attributes;

        std::vector<std::uint8_t> payload;
        if (entry.type == kEntryDirectory) {
            ++stats.directories;
        } else {
            ++stats.files;
            payload = readFileBytes(entry.absolutePath);
            meta.originalSize = static_cast<std::uint64_t>(payload.size());
            meta.checksum = fnv1aBytes(payload);
            stats.originalBytes += meta.originalSize;

            if (options.compress && !payload.empty()) {
                std::vector<std::uint8_t> compressed = rleCompress(payload);
                if (compressed.size() < payload.size()) {
                    payload.swap(compressed);
                    meta.flags |= kEntryFlagCompressed;
                }
            }
            if (!options.password.empty()) {
                cryptBuffer(payload, options.password, meta.path, meta.originalSize);
                meta.flags |= kEntryFlagEncrypted;
            }
            meta.storedSize = static_cast<std::uint64_t>(payload.size());
            stats.storedBytes += meta.storedSize;
        }

        writeEntryMeta(out, meta);
        if (!payload.empty()) {
            out.write(reinterpret_cast<const char*>(payload.data()),
                      static_cast<std::streamsize>(payload.size()));
            if (!out) {
                throw std::runtime_error("failed to write archive payload");
            }
        }
    }
    return stats;
}

OperationStats restoreBackup(const std::string& archiveFile,
                             const std::string& targetDir,
                             const RestoreOptions& options) {
    std::ifstream in(archiveFile, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open archive: " + archiveFile);
    }

    ArchiveHeader header = readHeader(in);
    validatePassword(header, options.password);
    std::string target = absolutePath(targetDir);
    if (!createDirectories(target)) {
        throw std::runtime_error("failed to create target directory: " + target);
    }

    struct DirectoryMeta {
        std::string path;
        DWORD attributes = 0;
        std::int64_t modifiedTime = 0;
    };
    std::vector<DirectoryMeta> directories;
    OperationStats stats;

    for (std::uint64_t i = 0; i < header.entryCount; ++i) {
        EntryMeta meta = readEntryMeta(in);
        std::string outputPath = joinPath(target, meta.path);

        if (meta.type == kEntryDirectory) {
            if (!createDirectories(outputPath)) {
                throw std::runtime_error("failed to create directory: " + outputPath);
            }
            directories.push_back({outputPath, meta.permissions, meta.modifiedTime});
            ++stats.directories;
            continue;
        }

        std::vector<std::uint8_t> payload = readExactBytes(in, meta.storedSize);
        payload = decodePayload(meta, std::move(payload), options.password);

        std::string parent = parentPath(outputPath);
        if (!parent.empty() && !createDirectories(parent)) {
            throw std::runtime_error("failed to create directory: " + parent);
        }
        if (pathExists(outputPath) && !options.overwrite) {
            throw std::runtime_error("target file exists; use --overwrite: " + outputPath);
        }
        writeFileBytes(outputPath, payload);
        applyMetadata(outputPath, meta.permissions, meta.modifiedTime);
        ++stats.files;
        stats.originalBytes += meta.originalSize;
        stats.storedBytes += meta.storedSize;
    }

    std::sort(directories.begin(), directories.end(), [](const DirectoryMeta& lhs,
                                                         const DirectoryMeta& rhs) {
        return lhs.path.size() > rhs.path.size();
    });
    for (const DirectoryMeta& directory : directories) {
        applyMetadata(directory.path, directory.attributes, directory.modifiedTime);
    }

    return stats;
}

std::vector<ArchiveEntryInfo> listArchive(const std::string& archiveFile) {
    std::ifstream in(archiveFile, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open archive: " + archiveFile);
    }
    ArchiveHeader header = readHeader(in);

    std::vector<ArchiveEntryInfo> entries;
    entries.reserve(static_cast<std::size_t>(header.entryCount));
    for (std::uint64_t i = 0; i < header.entryCount; ++i) {
        EntryMeta meta = readEntryMeta(in);
        ArchiveEntryInfo info;
        info.path = meta.path;
        info.type = entryTypeName(meta.type);
        info.originalSize = meta.originalSize;
        info.storedSize = meta.storedSize;
        info.modifiedTime = meta.modifiedTime;
        info.compressed = (meta.flags & kEntryFlagCompressed) != 0;
        info.encrypted = (meta.flags & kEntryFlagEncrypted) != 0;
        entries.push_back(info);

        if (meta.storedSize > 0) {
            if (meta.storedSize > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
                throw std::runtime_error("corrupt archive: payload too large");
            }
            in.seekg(static_cast<std::streamoff>(meta.storedSize), std::ios::cur);
            if (!in) {
                throw std::runtime_error("corrupt archive: unexpected end of file");
            }
        }
    }
    return entries;
}

}  // namespace sdb
