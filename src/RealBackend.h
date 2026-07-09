#pragma once

#include "BackendAdapter.h"
#include <atomic>
#include <memory>
#include <thread>

namespace pbackup::ui {

class RealBackend : public BackendAdapter {
public:
    RealBackend() = default;
    ~RealBackend() override;
    bool startBackup(const BackupRequest& req, const FilterSpec& filter) override;
    bool startRestore(const RestoreRequest& req) override;
    void cancel() override { m_cancel = true; }
    bool isRunning() const override { return m_running.load(); }

private:
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancel{false};
    std::thread m_thread;

    void runBackup(BackupRequest req, FilterSpec filter);
    void runRestore(RestoreRequest req);
};

// Declared here but defined in BackendAdapter.cpp
std::unique_ptr<BackendAdapter> createRealBackend();

} // namespace pbackup::ui