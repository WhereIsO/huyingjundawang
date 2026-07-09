// MainWindow.h — 主窗口：顶部标签页（备份/还原/筛选）+ 底部进度条与日志面板
#pragma once
#include <QMainWindow>
#include <memory>
#include "BackendAdapter.h"

class QTabWidget;
class QProgressBar;
class QLabel;

namespace pbackup::ui {

class BackupTab;
class RestoreTab;
class FilterTab;
class LogPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onBackupStart();
    void onRestoreStart();
    void onCancel();

    void onProgress(const Progress& p);
    void onLog(int level, const QString& text);
    void onFinished(bool success, const QString& summary);
    void onFailed(const QString& reason);

private:
    void setBusy(bool busy);
    void wireBackend();

    QTabWidget*  m_tabs    = nullptr;
    BackupTab*   m_backup  = nullptr;
    RestoreTab*  m_restore = nullptr;
    FilterTab*   m_filter  = nullptr;

    QProgressBar* m_progress = nullptr;
    QLabel*       m_status   = nullptr;
    LogPanel*     m_log      = nullptr;

    std::unique_ptr<BackendAdapter> m_backend;
};

} // namespace pbackup::ui
