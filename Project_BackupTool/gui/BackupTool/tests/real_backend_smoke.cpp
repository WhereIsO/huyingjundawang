#include "BackendAdapter.h"
#include "RealBackend.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QMetaType>
#include <QTimer>

#include <filesystem>
#include <fstream>

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
    bool finished = false;
    bool failed = false;
    bool success = false;
    int progressCount = 0;
    QString message;
};

AsyncResult waitForBackend(pbackup::ui::BackendAdapter* backend) {
    QEventLoop loop;
    AsyncResult result;
    QObject::connect(backend, &pbackup::ui::BackendAdapter::progress,
                     &loop, [&](const pbackup::ui::Progress&) {
                         ++result.progressCount;
                     });
    QObject::connect(backend, &pbackup::ui::BackendAdapter::finished,
                     &loop, [&](bool success, const QString& summary) {
                         result.finished = true;
                         result.success = success;
                         result.message = summary;
                         loop.quit();
                     });
    QObject::connect(backend, &pbackup::ui::BackendAdapter::failed,
                     &loop, [&](const QString& reason) {
                         result.failed = true;
                         result.message = reason;
                         loop.quit();
                     });
    QTimer::singleShot(10000, &loop, [&]() {
        result.failed = true;
        result.message = QStringLiteral("等待 RealBackend 超时");
        loop.quit();
    });
    loop.exec();
    return result;
}

} // namespace

TEST(RealBackendSmokeTest, FactoryReturnsRealBackendWhenEnvSet) {
    qputenv("BACKUP_BACKEND_MODE", "real");
    auto backend = pbackup::ui::createBackend();
    EXPECT_NE(dynamic_cast<pbackup::ui::RealBackend*>(backend.get()), nullptr);
}

TEST(RealBackendSmokeTest, BackupThenRestoreRoundTrip) {
    qputenv("BACKUP_BACKEND_MODE", "real");
    TempDir t;
    writeText(t.path() / "src" / "a.txt", "hello real backend");
    writeText(t.path() / "src" / "nested" / "b.txt", "nested");

    auto backend = pbackup::ui::createBackend();
    pbackup::ui::BackupRequest backup;
    backup.sourceDir = QString::fromStdWString((t.path() / "src").wstring());
    backup.outputPkg = QString::fromStdWString((t.path() / "sample.pbackup").wstring());
    backup.compress = true;
    backup.encrypt = true;
    backup.password = QStringLiteral("pw");

    pbackup::ui::FilterSpec filter;
    filter.nameGlob = QStringLiteral("*.txt");
    ASSERT_TRUE(backend->startBackup(backup, filter));
    AsyncResult backupResult = waitForBackend(backend.get());
    ASSERT_FALSE(backupResult.failed) << backupResult.message.toStdString();
    ASSERT_TRUE(backupResult.finished);
    ASSERT_TRUE(backupResult.success);
    EXPECT_GT(backupResult.progressCount, 0);
    EXPECT_TRUE(std::filesystem::exists(t.path() / "sample.pbackup"));
    EXPECT_FALSE(backend->isRunning());

    pbackup::ui::RestoreRequest restore;
    restore.pkg = backup.outputPkg;
    restore.destDir = QString::fromStdWString((t.path() / "dst").wstring());
    restore.password = QStringLiteral("pw");
    restore.overwrite = false;

    ASSERT_TRUE(backend->startRestore(restore));
    AsyncResult restoreResult = waitForBackend(backend.get());
    ASSERT_FALSE(restoreResult.failed) << restoreResult.message.toStdString();
    ASSERT_TRUE(restoreResult.finished);
    ASSERT_TRUE(restoreResult.success);
    EXPECT_GT(restoreResult.progressCount, 0);
    EXPECT_EQ(readText(t.path() / "dst" / "a.txt"), "hello real backend");
    EXPECT_EQ(readText(t.path() / "dst" / "nested" / "b.txt"), "nested");
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    qRegisterMetaType<pbackup::ui::Progress>("Progress");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
