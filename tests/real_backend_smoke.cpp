#include "BackendAdapter.h"
#include "RealBackend.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

namespace {

class TempDir {
public:
    TempDir() {
        auto base = std::filesystem::temp_directory_path();
        auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path_ = base / ("pbackup_real_backend_" + std::to_string(stamp));
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

struct AsyncResult {
    bool done = false;
    bool failed = false;
    bool success = false;
    int progressCount = 0;
    std::string message;
};

AsyncResult waitForBackend(pbackup::ui::BackendAdapter* backend) {
    std::mutex mutex;
    std::condition_variable cv;
    AsyncResult result;

    backend->setOnProgress([&](const pbackup::ui::Progress&) {
        std::lock_guard<std::mutex> lock(mutex);
        ++result.progressCount;
    });
    backend->setOnFinished([&](bool success, const std::string& summary) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            result.done = true;
            result.success = success;
            result.message = summary;
        }
        cv.notify_one();
    });
    backend->setOnFailed([&](const std::string& reason) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            result.done = true;
            result.failed = true;
            result.message = reason;
        }
        cv.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex);
    if (!cv.wait_for(lock, std::chrono::seconds(10), [&] { return result.done; })) {
        result.done = true;
        result.failed = true;
        result.message = "waiting for RealBackend timed out";
    }
    return result;
}

} // namespace

TEST(RealBackendSmokeTest, FactoryReturnsRealBackendByDefault) {
    auto backend = pbackup::ui::createBackend();
    EXPECT_NE(dynamic_cast<pbackup::ui::RealBackend*>(backend.get()), nullptr);
}

TEST(RealBackendSmokeTest, BackupThenRestoreRoundTrip) {
    TempDir t;
    writeText(t.path() / "src" / "a.txt", "hello real backend");
    writeText(t.path() / "src" / "nested" / "b.txt", "nested");

    auto backend = pbackup::ui::createBackend();
    pbackup::ui::BackupRequest backup;
    backup.sourceDir = (t.path() / "src").u8string();
    backup.outputPkg = (t.path() / "sample.pbackup").u8string();
    backup.compress = true;
    backup.encrypt = true;
    backup.password = "pw";

    pbackup::ui::FilterSpec filter;
    filter.nameGlob = "*.txt";
    ASSERT_TRUE(backend->startBackup(backup, filter));
    AsyncResult backupResult = waitForBackend(backend.get());
    ASSERT_FALSE(backupResult.failed) << backupResult.message;
    ASSERT_TRUE(backupResult.done);
    ASSERT_TRUE(backupResult.success);
    EXPECT_GT(backupResult.progressCount, 0);
    EXPECT_TRUE(std::filesystem::exists(t.path() / "sample.pbackup"));
    EXPECT_FALSE(backend->isRunning());

    pbackup::ui::RestoreRequest restore;
    restore.pkg = backup.outputPkg;
    restore.destDir = (t.path() / "dst").u8string();
    restore.password = "pw";
    restore.overwrite = false;

    ASSERT_TRUE(backend->startRestore(restore));
    AsyncResult restoreResult = waitForBackend(backend.get());
    ASSERT_FALSE(restoreResult.failed) << restoreResult.message;
    ASSERT_TRUE(restoreResult.done);
    ASSERT_TRUE(restoreResult.success);
    EXPECT_GT(restoreResult.progressCount, 0);
    EXPECT_EQ(readText(t.path() / "dst" / "a.txt"), "hello real backend");
    EXPECT_EQ(readText(t.path() / "dst" / "nested" / "b.txt"), "nested");
}
