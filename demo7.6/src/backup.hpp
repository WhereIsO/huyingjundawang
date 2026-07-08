#ifndef SDB_BACKUP_HPP
#define SDB_BACKUP_HPP

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace sdb {

struct FilterOptions {
    std::vector<std::string> includeExtensions;
    std::vector<std::string> excludeExtensions;
    std::string nameContains;
    std::uint64_t minSize = 0;
    std::uint64_t maxSize = std::numeric_limits<std::uint64_t>::max();
    bool hasModifiedAfter = false;
    std::int64_t modifiedAfter = 0;
};

struct BackupOptions {
    bool compress = false;
    std::string password;
    FilterOptions filter;
};

struct RestoreOptions {
    std::string password;
    bool overwrite = false;
};

struct OperationStats {
    std::uint64_t directories = 0;
    std::uint64_t files = 0;
    std::uint64_t originalBytes = 0;
    std::uint64_t storedBytes = 0;
};

struct ArchiveEntryInfo {
    std::string path;
    std::string type;
    std::uint64_t originalSize = 0;
    std::uint64_t storedSize = 0;
    std::int64_t modifiedTime = 0;
    bool compressed = false;
    bool encrypted = false;
};

OperationStats createBackup(const std::string& sourceDir,
                            const std::string& archiveFile,
                            const BackupOptions& options);

OperationStats restoreBackup(const std::string& archiveFile,
                             const std::string& targetDir,
                             const RestoreOptions& options);

std::vector<ArchiveEntryInfo> listArchive(const std::string& archiveFile);

bool parseDate(const std::string& value, std::int64_t& timestamp);
std::vector<std::string> splitCommaList(const std::string& value);

}  // namespace sdb

#endif  // SDB_BACKUP_HPP
