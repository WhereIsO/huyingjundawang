#include "backup.hpp"

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void printUsage() {
    std::cout
        << "Simple Data Backup (sdb)\n"
        << "\n"
        << "Usage:\n"
        << "  sdb backup <source_dir> <archive.sdb> [options]\n"
        << "  sdb restore <archive.sdb> <target_dir> [options]\n"
        << "  sdb list <archive.sdb>\n"
        << "\n"
        << "Backup options:\n"
        << "  --compress                 enable built-in RLE compression\n"
        << "  --password <text>          encrypt file payloads with a password\n"
        << "  --include-ext <list>       comma-separated extensions, e.g. .cpp,.h\n"
        << "  --exclude-ext <list>       comma-separated extensions, e.g. .tmp,.log\n"
        << "  --name-contains <text>     only include files whose names contain text\n"
        << "  --min-size <bytes>         only include files >= bytes\n"
        << "  --max-size <bytes>         only include files <= bytes\n"
        << "  --modified-after <date>    only include files modified after YYYY-MM-DD\n"
        << "\n"
        << "Restore options:\n"
        << "  --password <text>          password for encrypted archives\n"
        << "  --overwrite                replace existing files\n";
}

std::uint64_t parseBytes(const std::string& value, const std::string& option) {
    std::size_t consumed = 0;
    std::uint64_t number = 0;
    try {
        number = std::stoull(value, &consumed, 10);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid value for " + option + ": " + value);
    }
    if (consumed != value.size()) {
        throw std::runtime_error("invalid value for " + option + ": " + value);
    }
    return number;
}

std::string requireValue(int argc, char* argv[], int& index, const std::string& option) {
    if (index + 1 >= argc) {
        throw std::runtime_error("missing value for " + option);
    }
    ++index;
    return argv[index];
}

std::string formatTime(std::int64_t value) {
    if (value <= 0) {
        return "-";
    }
    std::time_t raw = static_cast<std::time_t>(value);
    std::tm* local = std::localtime(&raw);
    if (local == nullptr) {
        return "-";
    }
    std::ostringstream out;
    out << std::put_time(local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

int handleBackup(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage();
        return 1;
    }

    sdb::BackupOptions options;
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--compress") {
            options.compress = true;
        } else if (arg == "--password") {
            options.password = requireValue(argc, argv, i, arg);
        } else if (arg == "--include-ext") {
            options.filter.includeExtensions = sdb::splitCommaList(requireValue(argc, argv, i, arg));
        } else if (arg == "--exclude-ext") {
            options.filter.excludeExtensions = sdb::splitCommaList(requireValue(argc, argv, i, arg));
        } else if (arg == "--name-contains") {
            options.filter.nameContains = requireValue(argc, argv, i, arg);
        } else if (arg == "--min-size") {
            options.filter.minSize = parseBytes(requireValue(argc, argv, i, arg), arg);
        } else if (arg == "--max-size") {
            options.filter.maxSize = parseBytes(requireValue(argc, argv, i, arg), arg);
        } else if (arg == "--modified-after") {
            std::string value = requireValue(argc, argv, i, arg);
            options.filter.hasModifiedAfter = true;
            if (!sdb::parseDate(value, options.filter.modifiedAfter)) {
                throw std::runtime_error("invalid date for --modified-after: " + value);
            }
        } else {
            throw std::runtime_error("unknown option: " + arg);
        }
    }

    sdb::OperationStats stats = sdb::createBackup(argv[2], argv[3], options);
    std::cout << "Backup created: " << argv[3] << "\n"
              << "Directories: " << stats.directories << "\n"
              << "Files: " << stats.files << "\n"
              << "Original bytes: " << stats.originalBytes << "\n"
              << "Stored bytes: " << stats.storedBytes << "\n";
    return 0;
}

int handleRestore(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage();
        return 1;
    }

    sdb::RestoreOptions options;
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--password") {
            options.password = requireValue(argc, argv, i, arg);
        } else if (arg == "--overwrite") {
            options.overwrite = true;
        } else {
            throw std::runtime_error("unknown option: " + arg);
        }
    }

    sdb::OperationStats stats = sdb::restoreBackup(argv[2], argv[3], options);
    std::cout << "Archive restored to: " << argv[3] << "\n"
              << "Directories: " << stats.directories << "\n"
              << "Files: " << stats.files << "\n"
              << "Restored bytes: " << stats.originalBytes << "\n";
    return 0;
}

int handleList(int argc, char* argv[]) {
    if (argc != 3) {
        printUsage();
        return 1;
    }
    std::vector<sdb::ArchiveEntryInfo> entries = sdb::listArchive(argv[2]);
    std::cout << "Type       Original    Stored      Modified             Flags  Path\n";
    for (const sdb::ArchiveEntryInfo& entry : entries) {
        std::string flags;
        if (entry.compressed) {
            flags += "C";
        }
        if (entry.encrypted) {
            flags += "E";
        }
        if (flags.empty()) {
            flags = "-";
        }
        std::cout << std::left << std::setw(10) << entry.type << std::right << std::setw(11)
                  << entry.originalSize << " " << std::setw(10) << entry.storedSize << "  "
                  << formatTime(entry.modifiedTime) << "  " << std::setw(5) << flags << "  "
                  << entry.path << "\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            printUsage();
            return 1;
        }

        std::string command = argv[1];
        if (command == "backup") {
            return handleBackup(argc, argv);
        }
        if (command == "restore") {
            return handleRestore(argc, argv);
        }
        if (command == "list") {
            return handleList(argc, argv);
        }
        if (command == "help" || command == "--help" || command == "-h") {
            printUsage();
            return 0;
        }

        throw std::runtime_error("unknown command: " + command);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2;
    }
}
