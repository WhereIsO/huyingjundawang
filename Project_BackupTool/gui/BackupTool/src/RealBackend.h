#pragma once

#include "BackendAdapter.h"

#include <atomic>

namespace pbackup::ui {

class RealBackend : public BackendAdapter {
public:
    explicit RealBackend(QObject* parent = nullptr);
    bool startBackup(const BackupRequest& req, const FilterSpec& filter) override;
    bool startRestore(const RestoreRequest& req) override;
    void cancel() override;
    bool isRunning() const override { return m_running.load(); }

private:
    std::atomic_bool m_running{false};
    std::atomic_bool m_cancel{false};

    void runBackup(BackupRequest req, FilterSpec filter);
    void runRestore(RestoreRequest req);
};

} // namespace pbackup::ui
