#include "BackendAdapter.h"
#include "RealBackend.h"
#include "core/types.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

namespace pbackup::ui {
namespace fs = std::filesystem;

// ------------------------------------------------------------------
// Helper: recursive directory size and file count
// ------------------------------------------------------------------
static std::uint64_t dirSize(const std::string& path) {
    std::uint64_t total = 0;
    try {
        for (auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file())
                total += static_cast<std::uint64_t>(entry.file_size());
        }
    } catch (...) {}
    return total;
}

static std::uint32_t countFiles(const std::string& path) {
    std::uint32_t total = 0;
    try {
        for (auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) ++total;
        }
    } catch (...) {}
    return total;
}

// ------------------------------------------------------------------
// MockBackend
// ------------------------------------------------------------------
class MockBackend : public BackendAdapter {
public:
    bool startBackup(const BackupRequest& req, const FilterSpec&) override;
    bool startRestore(const RestoreRequest& req) override;
    void cancel() override { m_cancel = true; }
    bool isRunning() const override { return m_running.load(); }

private:
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancel{false};
    std::thread m_thread;

    void runMockBackup(BackupRequest req);
    void runMockRestore(RestoreRequest req);
};

bool MockBackend::startBackup(const BackupRequest& req, const FilterSpec&) {
    if (m_running.load()) return false;
    m_running = true;
    m_cancel = false;
    if (m_thread.joinable()) m_thread.join();
    m_thread = std::thread(&MockBackend::runMockBackup, this, req);
    return true;
}

bool MockBackend::startRestore(const RestoreRequest& req) {
    if (m_running.load()) return false;
    m_running = true;
    m_cancel = false;
    if (m_thread.joinable()) m_thread.join();
    m_thread = std::thread(&MockBackend::runMockRestore, this, req);
    return true;
}

void MockBackend::runMockBackup(BackupRequest req) {
    emitLog(0, "开始备份(Mock): " + req.sourceDir + " -> " + req.outputPkg);
    if (!fs::exists(req.sourceDir)) {
        emitFailed("源目录不存在: " + req.sourceDir);
        m_running = false;
        return;
    }
    try {
        fs::create_directories(fs::path(req.outputPkg).parent_path());
    } catch (...) {}

    Progress p;
    p.stage = "Scanning";
    p.totalBytes = dirSize(req.sourceDir);
    p.totalFiles = countFiles(req.sourceDir);
    if (p.totalBytes == 0) p.totalBytes = 1;
    emitProgress(p);

    const int totalSteps = 100;
    for (int i = 1; i <= totalSteps; ++i) {
        if (m_cancel.load()) {
            emitFailed("已取消");
            m_running = false;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        p.doneBytes = static_cast<std::uint64_t>(
            static_cast<double>(p.totalBytes) * i / totalSteps);
        p.doneFiles = static_cast<std::uint32_t>(
            static_cast<double>(p.totalFiles) * i / totalSteps);
        p.currentFile = "src/file_" + std::to_string(i) + ".dat";
        if (i < 30) p.stage = "Scanning";
        else if (i < 50) p.stage = "Filtering";
        else if (i < 95) p.stage = "Writing";
        else p.stage = "Verifying";
        emitProgress(p);
    }

    // Write a small mock file
    try {
        std::ofstream ofs(req.outputPkg, std::ios::binary);
        ofs.write("PBACKUP\x01\x00\x00\x00MOCK", 14);
    } catch (...) {}

    emitLog(3, "备份完成(Mock): " + req.outputPkg);
    emitFinished(true, "Mock 备份完成，共处理 " + std::to_string(p.totalFiles) + " 个文件");
    m_running = false;
}

void MockBackend::runMockRestore(RestoreRequest req) {
    emitLog(0, "开始还原(Mock): " + req.pkg + " -> " + req.destDir);
    if (!fs::exists(req.pkg)) {
        emitFailed("备份包不存在: " + req.pkg);
        m_running = false;
        return;
    }
    try {
        fs::create_directories(req.destDir);
    } catch (...) {}

    Progress p;
    p.stage = "Restoring";
    p.totalBytes = 100;
    p.totalFiles = 1;
    for (int i = 1; i <= 100; ++i) {
        if (m_cancel.load()) {
            emitFailed("已取消");
            m_running = false;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        p.doneBytes = static_cast<std::uint64_t>(i);
        p.doneFiles = (i == 100) ? 1u : 0u;
        p.currentFile = "dst/restored_" + std::to_string(i) + ".dat";
        emitProgress(p);
    }

    emitLog(3, "还原完成(Mock)");
    emitFinished(true, "Mock 还原完成");
    m_running = false;
}

// ------------------------------------------------------------------
// Factory
// ------------------------------------------------------------------
std::unique_ptr<BackendAdapter> createBackend() {
    const char* mode = std::getenv("BACKUP_BACKEND_MODE");
    if (mode) {
        std::string m(mode);
        std::transform(m.begin(), m.end(), m.begin(), ::tolower);
        if (m == "mock") {
            return std::make_unique<MockBackend>();
        }
    }
    return createRealBackend();
}

} // namespace pbackup::ui