// BackendAdapter.h — Abstract backend interface (no Qt dependency)
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace pbackup::ui {

struct BackupRequest {
    std::string sourceDir;
    std::string outputPkg;
    bool compress   = true;
    bool encrypt    = false;
    std::string password;
};

struct RestoreRequest {
    std::string pkg;
    std::string destDir;
    std::string password;
    bool overwrite = false;
};

struct FilterSpec {
    std::vector<std::string> includePath;
    std::vector<std::string> excludePath;
    std::string nameGlob;
    std::string typeFilter;
    std::string sizeMin;
    std::string sizeMax;
    std::string mtimeAfter;
    std::string mtimeBefore;
    std::string ownerSid;
};

struct Progress {
    std::uint64_t totalBytes  = 0;
    std::uint64_t doneBytes   = 0;
    std::uint32_t totalFiles  = 0;
    std::uint32_t doneFiles   = 0;
    std::string   currentFile;
    std::string   stage;
    std::vector<std::string> warnings;
    int percent() const {
        if (totalBytes == 0) return 0;
        return static_cast<int>(static_cast<double>(doneBytes) * 100.0 / static_cast<double>(totalBytes));
    }
};

class BackendAdapter {
public:
    virtual ~BackendAdapter() = default;

    using ProgressCallback = std::function<void(const Progress&)>;
    using LogCallback      = std::function<void(int level, const std::string& text)>;
    using FinishedCallback = std::function<void(bool success, const std::string& summary)>;
    using FailedCallback   = std::function<void(const std::string& reason)>;

    // Set callbacks — called from background threads, must be thread-safe
    void setOnProgress(ProgressCallback cb)  { m_onProgress = std::move(cb); }
    void setOnLog(LogCallback cb)            { m_onLog = std::move(cb); }
    void setOnFinished(FinishedCallback cb)  { m_onFinished = std::move(cb); }
    void setOnFailed(FailedCallback cb)      { m_onFailed = std::move(cb); }

    // Returns true if the task was started; callbacks provide async results
    virtual bool startBackup(const BackupRequest& req, const FilterSpec& filter) = 0;
    virtual bool startRestore(const RestoreRequest& req) = 0;
    virtual void cancel() = 0;
    virtual bool isRunning() const = 0;

protected:
    // Invoke these from background threads
    void emitProgress(const Progress& p)    { if (m_onProgress) m_onProgress(p); }
    void emitLog(int level, const std::string& text) { if (m_onLog) m_onLog(level, text); }
    void emitFinished(bool ok, const std::string& s) { if (m_onFinished) m_onFinished(ok, s); }
    void emitFailed(const std::string& r)   { if (m_onFailed) m_onFailed(r); }

    ProgressCallback   m_onProgress;
    LogCallback        m_onLog;
    FinishedCallback   m_onFinished;
    FailedCallback     m_onFailed;
};

std::unique_ptr<BackendAdapter> createBackend();

} // namespace pbackup::ui