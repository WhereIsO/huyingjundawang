#include "core/archive.h"
#include "core/backup_task.h"
#include "core/checksum.h"
#include "core/crypto.h"
#include "core/encoding.h"
#include "core/filter.h"
#include "core/huffman.h"
#include "core/metadata.h"
#include "core/scanner.h"

#include <gtest/gtest.h>

#include <Windows.h>

#include <chrono>
#include <fstream>
#include <random>

using namespace pbackup::core;

namespace {

class TempDir {
public:
    TempDir() {
        auto base = std::filesystem::temp_directory_path();
        auto tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path_ = base / ("pbackup_test_" + std::to_string(tick));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void writeText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string readText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

std::vector<std::uint8_t> bytes(std::string s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

FileEntry sampleEntry(std::string rel, EntryType type = EntryType::File,
                      std::uint64_t size = 0) {
    FileEntry e;
    e.relPath = std::move(rel);
    e.type = type;
    e.meta.size = size;
    e.times.mtimeNs = 1000000000LL;
    e.meta.ownerSid = "S-1-5-21-test";
    return e;
}

void backupDir(const std::filesystem::path& src, const std::filesystem::path& pkg,
               bool compress = true, bool encrypt = false,
               std::string password = {}, FilterRules filters = {}) {
    BackupOptions opt;
    opt.sourceRoot = src;
    opt.outputPkg = pkg;
    opt.compress = compress;
    opt.encrypt = encrypt;
    opt.password = password.empty() ? std::nullopt : std::optional<std::string>(password);
    opt.filters = std::move(filters);
    BackupTask(std::move(opt)).run();
}

void restorePkg(const std::filesystem::path& pkg, const std::filesystem::path& dst,
                bool overwrite = false, std::string password = {}) {
    RestoreOptions opt;
    opt.pkg = pkg;
    opt.destRoot = dst;
    opt.overwrite = overwrite;
    opt.password = password.empty() ? std::nullopt : std::optional<std::string>(password);
    RestoreTask(std::move(opt)).run();
}

} // namespace

TEST(HuffmanTest, EmptyRoundTrip) {
    EXPECT_TRUE(huffmanDecompress(huffmanCompress({})).empty());
}

TEST(HuffmanTest, TextRoundTrip) {
    const auto input = bytes("hello hello hello backup tool");
    EXPECT_EQ(huffmanDecompress(huffmanCompress(input)), input);
}

TEST(HuffmanTest, SingleSymbolRoundTrip) {
    std::vector<std::uint8_t> input(4096, 0x5A);
    EXPECT_EQ(huffmanDecompress(huffmanCompress(input)), input);
}

TEST(HuffmanTest, BinaryRoundTrip) {
    std::vector<std::uint8_t> input;
    for (int i = 0; i < 1024; ++i) input.push_back(static_cast<std::uint8_t>(i % 251));
    EXPECT_EQ(huffmanDecompress(huffmanCompress(input)), input);
}

TEST(HuffmanTest, RejectInvalidHeader) {
    EXPECT_THROW(huffmanDecompress(bytes("bad")), BackupError);
}

TEST(CryptoTest, SaltHasEightBytes) {
    EXPECT_EQ(randomSalt8().size(), 8U);
}

TEST(CryptoTest, EncryptDecryptRoundTrip) {
    const auto salt = randomSalt8();
    const auto input = bytes("secret payload");
    const auto encrypted = aesGcmEncrypt(input, "pw", salt, 1000);
    EXPECT_EQ(aesGcmDecrypt(encrypted, "pw", salt, 1000), input);
}

TEST(CryptoTest, WrongPasswordFails) {
    const auto salt = randomSalt8();
    const auto encrypted = aesGcmEncrypt(bytes("secret"), "right", salt, 1000);
    EXPECT_THROW(aesGcmDecrypt(encrypted, "wrong", salt, 1000), BackupError);
}

TEST(CryptoTest, PackUnpackEncryptedPayload) {
    const auto salt = randomSalt8();
    const auto encrypted = aesGcmEncrypt(bytes("abc"), "pw", salt, 1000);
    EXPECT_EQ(aesGcmDecrypt(unpackEncryptedPayload(packEncryptedPayload(encrypted)),
                            "pw", salt, 1000),
              bytes("abc"));
}

TEST(CryptoTest, EmptyPasswordRejected) {
    const auto salt = randomSalt8();
    EXPECT_THROW(aesGcmEncrypt(bytes("x"), "", salt, 1000), BackupError);
}

TEST(FilterTest, IncludePathMatches) {
    FilterRules r;
    r.includePath = {"docs"};
    EXPECT_TRUE(matchesFilter(sampleEntry("docs\\a.txt"), r));
    EXPECT_FALSE(matchesFilter(sampleEntry("src\\a.txt"), r));
}

TEST(FilterTest, ExcludePathRejects) {
    FilterRules r;
    r.excludePath = {"tmp"};
    EXPECT_FALSE(matchesFilter(sampleEntry("tmp\\a.txt"), r));
    EXPECT_TRUE(matchesFilter(sampleEntry("src\\a.txt"), r));
}

TEST(FilterTest, NameGlobMatches) {
    FilterRules r;
    r.nameGlob = "*.txt";
    EXPECT_TRUE(matchesFilter(sampleEntry("a\\b.txt"), r));
    EXPECT_FALSE(matchesFilter(sampleEntry("a\\b.bin"), r));
}

TEST(FilterTest, TypeFilterMatches) {
    FilterRules r;
    r.typeFilter = parseTypeFilter("file,symlink");
    EXPECT_TRUE(matchesFilter(sampleEntry("a", EntryType::Symlink), r));
    EXPECT_FALSE(matchesFilter(sampleEntry("a", EntryType::Dir), r));
}

TEST(FilterTest, SizeRangeMatches) {
    FilterRules r;
    r.sizeMin = 10;
    r.sizeMax = 20;
    EXPECT_TRUE(matchesFilter(sampleEntry("a", EntryType::File, 15), r));
    EXPECT_FALSE(matchesFilter(sampleEntry("a", EntryType::File, 30), r));
}

TEST(FilterTest, OwnerMatches) {
    FilterRules r;
    r.ownerSid = "S-1-5-21-test";
    EXPECT_TRUE(matchesFilter(sampleEntry("a"), r));
    r.ownerSid = "S-1-5-21-other";
    EXPECT_FALSE(matchesFilter(sampleEntry("a"), r));
}

TEST(FilterTest, DateParserAcceptsValidDate) {
    EXPECT_TRUE(parseOptionalDateToNs("2026-07-06", false).has_value());
}

TEST(FilterTest, BadTypeFilterThrows) {
    EXPECT_THROW(parseTypeFilter("unknown"), BackupError);
}

TEST(ArchiveTest, PlainArchiveRoundTrip) {
    TempDir t;
    std::vector<ArchiveRecord> records;
    records.push_back({sampleEntry("a.txt", EntryType::File, 3), bytes("abc")});
    ArchiveOptions opt;
    opt.compress = false;
    writeArchive(t.path() / "a.pbackup", records, opt);
    auto out = readArchive(t.path() / "a.pbackup", "");
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(out[0].data, bytes("abc"));
}

TEST(ArchiveTest, CompressedArchiveRoundTrip) {
    TempDir t;
    std::vector<ArchiveRecord> records;
    records.push_back({sampleEntry("a.txt", EntryType::File, 100), bytes("aaaaaaaaaaaaaaaaaaaa")});
    ArchiveOptions opt;
    opt.compress = true;
    writeArchive(t.path() / "a.pbackup", records, opt);
    EXPECT_EQ(readArchive(t.path() / "a.pbackup", "")[0].data, bytes("aaaaaaaaaaaaaaaaaaaa"));
}

TEST(ArchiveTest, EncryptedArchiveRoundTrip) {
    TempDir t;
    std::vector<ArchiveRecord> records;
    records.push_back({sampleEntry("a.txt", EntryType::File, 6), bytes("secret")});
    ArchiveOptions opt;
    opt.compress = false;
    opt.encrypt = true;
    opt.password = "pw";
    opt.kdfIters = 1000;
    writeArchive(t.path() / "a.pbackup", records, opt);
    EXPECT_EQ(readArchive(t.path() / "a.pbackup", "pw")[0].data, bytes("secret"));
}

TEST(ArchiveTest, EncryptedArchiveWrongPasswordFails) {
    TempDir t;
    std::vector<ArchiveRecord> records;
    records.push_back({sampleEntry("a.txt", EntryType::File, 6), bytes("secret")});
    ArchiveOptions opt;
    opt.encrypt = true;
    opt.password = "pw";
    opt.kdfIters = 1000;
    writeArchive(t.path() / "a.pbackup", records, opt);
    EXPECT_THROW(readArchive(t.path() / "a.pbackup", "bad"), BackupError);
}

TEST(ArchiveTest, CorruptedArchiveFails) {
    TempDir t;
    const auto pkg = t.path() / "a.pbackup";
    std::vector<ArchiveRecord> records;
    records.push_back({sampleEntry("a.txt", EntryType::File, 3), bytes("abc")});
    ArchiveOptions opt;
    writeArchive(pkg, records, opt);
    std::fstream f(pkg, std::ios::binary | std::ios::in | std::ios::out);
    f.seekg(80);
    char old = 0;
    f.read(&old, 1);
    old = static_cast<char>(old ^ 0x7F);
    f.seekp(80);
    f.write(&old, 1);
    f.close();
    EXPECT_THROW(readArchive(pkg, ""), BackupError);
}

TEST(BackupRestoreTest, OrdinaryFileRoundTrip) {
    TempDir t;
    writeText(t.path() / "src" / "a.txt", "hello");
    backupDir(t.path() / "src", t.path() / "a.pbackup", false);
    restorePkg(t.path() / "a.pbackup", t.path() / "dst");
    EXPECT_EQ(readText(t.path() / "dst" / "a.txt"), "hello");
}

TEST(BackupRestoreTest, NestedDirectoryRoundTrip) {
    TempDir t;
    writeText(t.path() / "src" / "a" / "b" / "c.txt", "nested");
    backupDir(t.path() / "src", t.path() / "a.pbackup");
    restorePkg(t.path() / "a.pbackup", t.path() / "dst");
    EXPECT_EQ(readText(t.path() / "dst" / "a" / "b" / "c.txt"), "nested");
}

TEST(BackupRestoreTest, EmptyFileRoundTrip) {
    TempDir t;
    writeText(t.path() / "src" / "empty.txt", "");
    backupDir(t.path() / "src", t.path() / "a.pbackup");
    restorePkg(t.path() / "a.pbackup", t.path() / "dst");
    EXPECT_TRUE(std::filesystem::exists(t.path() / "dst" / "empty.txt"));
    EXPECT_EQ(std::filesystem::file_size(t.path() / "dst" / "empty.txt"), 0U);
}

TEST(BackupRestoreTest, EmptyDirectoryRoundTrip) {
    TempDir t;
    std::filesystem::create_directories(t.path() / "src" / "emptydir");
    backupDir(t.path() / "src", t.path() / "a.pbackup");
    restorePkg(t.path() / "a.pbackup", t.path() / "dst");
    EXPECT_TRUE(std::filesystem::is_directory(t.path() / "dst" / "emptydir"));
}

TEST(BackupRestoreTest, ChinesePathRoundTrip) {
    TempDir t;
    writeText(t.path() / L"src" / L"中文.txt", "utf8");
    backupDir(t.path() / "src", t.path() / "a.pbackup");
    restorePkg(t.path() / "a.pbackup", t.path() / "dst");
    EXPECT_EQ(readText(t.path() / L"dst" / L"中文.txt"), "utf8");
}

TEST(BackupRestoreTest, NameFilterExcludesNonMatchingFiles) {
    TempDir t;
    writeText(t.path() / "src" / "keep.txt", "yes");
    writeText(t.path() / "src" / "drop.bin", "no");
    FilterRules rules;
    rules.nameGlob = "*.txt";
    backupDir(t.path() / "src", t.path() / "a.pbackup", true, false, "", rules);
    restorePkg(t.path() / "a.pbackup", t.path() / "dst");
    EXPECT_TRUE(std::filesystem::exists(t.path() / "dst" / "keep.txt"));
    EXPECT_FALSE(std::filesystem::exists(t.path() / "dst" / "drop.bin"));
}

TEST(BackupRestoreTest, EncryptedBackupWrongPasswordFails) {
    TempDir t;
    writeText(t.path() / "src" / "a.txt", "secret");
    backupDir(t.path() / "src", t.path() / "a.pbackup", true, true, "pw");
    EXPECT_THROW(restorePkg(t.path() / "a.pbackup", t.path() / "dst", false, "bad"), BackupError);
}

TEST(BackupRestoreTest, OverwriteFalseRejectsExistingFile) {
    TempDir t;
    writeText(t.path() / "src" / "a.txt", "new");
    writeText(t.path() / "dst" / "a.txt", "old");
    backupDir(t.path() / "src", t.path() / "a.pbackup");
    EXPECT_THROW(restorePkg(t.path() / "a.pbackup", t.path() / "dst", false), BackupError);
}

TEST(BackupRestoreTest, OverwriteTrueReplacesExistingFile) {
    TempDir t;
    writeText(t.path() / "src" / "a.txt", "new");
    writeText(t.path() / "dst" / "a.txt", "old");
    backupDir(t.path() / "src", t.path() / "a.pbackup");
    restorePkg(t.path() / "a.pbackup", t.path() / "dst", true);
    EXPECT_EQ(readText(t.path() / "dst" / "a.txt"), "new");
}

TEST(BackupRestoreTest, RejectsPathTraversalInArchive) {
    TempDir t;
    std::vector<ArchiveRecord> records;
    records.push_back({sampleEntry("..\\escape.txt", EntryType::File, 6), bytes("escape")});
    ArchiveOptions opt;
    opt.compress = false;
    writeArchive(t.path() / "evil.pbackup", records, opt);

    EXPECT_THROW(restorePkg(t.path() / "evil.pbackup", t.path() / "dst"), BackupError);
    EXPECT_FALSE(std::filesystem::exists(t.path() / "escape.txt"));
}

TEST(ScannerTest, ScansRegularFiles) {
    TempDir t;
    writeText(t.path() / "src" / "a.txt", "x");
    RunContext ctx;
    auto entries = scanDirectoryTree(t.path() / "src", ctx);
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries[0].type, EntryType::File);
}

TEST(ScannerTest, DetectsHardlinkWhenSupported) {
    TempDir t;
    writeText(t.path() / "src" / "a.txt", "x");
    std::filesystem::create_directories(t.path() / "src");
    if (!CreateHardLinkW((t.path() / "src" / "b.txt").wstring().c_str(),
                         (t.path() / "src" / "a.txt").wstring().c_str(), nullptr)) {
        GTEST_SKIP() << "当前环境不允许创建硬链接";
    }
    RunContext ctx;
    auto entries = scanDirectoryTree(t.path() / "src", ctx);
    bool hasHardlink = false;
    for (const auto& e : entries) hasHardlink = hasHardlink || e.type == EntryType::Hardlink;
    EXPECT_TRUE(hasHardlink);
}

TEST(ScannerTest, DetectsSymlinkWhenSupported) {
    TempDir t;
    writeText(t.path() / "src" / "target.txt", "target");
    const auto link = t.path() / "src" / "link.txt";
    if (!CreateSymbolicLinkW(link.wstring().c_str(),
                             (t.path() / "src" / "target.txt").wstring().c_str(), 0)) {
        GTEST_SKIP() << "当前环境不允许创建符号链接";
    }
    RunContext ctx;
    auto entries = scanDirectoryTree(t.path() / "src", ctx);
    bool hasSymlink = false;
    for (const auto& e : entries) hasSymlink = hasSymlink || e.type == EntryType::Symlink;
    EXPECT_TRUE(hasSymlink);
}

TEST(BackupRestoreTest, SymlinkRoundTripWhenSupported) {
    TempDir t;
    writeText(t.path() / "src" / "target.txt", "target");
    const auto link = t.path() / "src" / "link.txt";
    if (!CreateSymbolicLinkW(link.wstring().c_str(),
                             (t.path() / "src" / "target.txt").wstring().c_str(), 0)) {
        GTEST_SKIP() << "当前环境不允许创建符号链接";
    }
    backupDir(t.path() / "src", t.path() / "a.pbackup", true);
    restorePkg(t.path() / "a.pbackup", t.path() / "dst");
    std::error_code ec;
    EXPECT_TRUE(std::filesystem::is_symlink(std::filesystem::symlink_status(t.path() / "dst" / "link.txt", ec)));
}

TEST(ScannerTest, DetectsJunctionWhenSupported) {
    TempDir t;
    std::filesystem::create_directories(t.path() / "src");
    std::filesystem::create_directories(t.path() / "target");
    std::string error;
    if (!createJunctionBestEffort(t.path() / "src" / "junction", t.path() / "target", error)) {
        GTEST_SKIP() << error;
    }
    RunContext ctx;
    auto entries = scanDirectoryTree(t.path() / "src", ctx);
    bool hasJunction = false;
    for (const auto& e : entries) {
        hasJunction = hasJunction || (e.type == EntryType::Junction && !e.meta.targetPath.empty());
    }
    EXPECT_TRUE(hasJunction);
}

TEST(BackupRestoreTest, JunctionRoundTripWhenSupported) {
    TempDir t;
    std::filesystem::create_directories(t.path() / "src");
    std::filesystem::create_directories(t.path() / "target");
    std::string error;
    if (!createJunctionBestEffort(t.path() / "src" / "junction", t.path() / "target", error)) {
        GTEST_SKIP() << error;
    }
    backupDir(t.path() / "src", t.path() / "a.pbackup", true);
    const auto records = readArchive(t.path() / "a.pbackup", "");
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].entry.type, EntryType::Junction);
    EXPECT_EQ(records[0].entry.relPath, "junction");
    EXPECT_FALSE(records[0].entry.meta.targetPath.empty());
    RestoreOptions opt;
    opt.pkg = t.path() / "a.pbackup";
    opt.destRoot = t.path() / "dst";
    std::vector<std::string> warnings;
    RunContext ctx;
    ctx.progress = [&warnings](const Progress& p) {
        warnings = p.warnings;
        return true;
    };
    RestoreTask(std::move(opt)).run(ctx);
    if ((queryAttributes(t.path() / "dst" / "junction") & FILE_ATTRIBUTE_REPARSE_POINT) == 0U) {
        std::string joined;
        for (const auto& w : warnings) joined += w + "\n";
        ADD_FAILURE() << "relPath=" << records[0].entry.relPath
                      << "\ntargetPath=" << records[0].entry.meta.targetPath
                      << "\nattributes=" << queryAttributes(t.path() / "dst" / "junction")
                      << "\nexists=" << std::filesystem::exists(t.path() / "dst" / "junction")
                      << "\nwarnings=\n" << joined;
    }
}

TEST(MetadataTest, AttributesRoundTripReadonly) {
    TempDir t;
    const auto file = t.path() / "a.txt";
    writeText(file, "x");
    SetFileAttributesW(file.wstring().c_str(), FILE_ATTRIBUTE_READONLY);
    EXPECT_NE(queryAttributes(file) & FILE_ATTRIBUTE_READONLY, 0U);
    SetFileAttributesW(file.wstring().c_str(), FILE_ATTRIBUTE_NORMAL);
}

TEST(ProgressTest, BackupInvokesProgressCallback) {
    TempDir t;
    writeText(t.path() / "src" / "a.txt", "x");
    BackupOptions opt;
    opt.sourceRoot = t.path() / "src";
    opt.outputPkg = t.path() / "a.pbackup";
    int calls = 0;
    RunContext ctx;
    ctx.progress = [&calls](const Progress&) {
        ++calls;
        return true;
    };
    BackupTask(std::move(opt)).run(ctx);
    EXPECT_GT(calls, 0);
}
