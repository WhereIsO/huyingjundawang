// MainWindow.h — 主窗口：标签页 + 独立进度窗口
#pragma once
 #include <QMainWindow>
 #include <memory>
 #include "BackendAdapter.h"
 #include "SessionHistory.h"

 class QTabWidget;
 class QLabel;
 class QProgressBar;
 class QCloseEvent;

namespace pbackup::ui {

class BackupTab;
class RestoreTab;
class FilterTab;
class ProgressDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
     explicit MainWindow(QWidget* parent = nullptr);
 
 protected:
     void closeEvent(QCloseEvent* event) override;

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
    void showCompletionSummary(bool success, const QString& summary);
    QTabWidget*   m_tabs    = nullptr;
    BackupTab*    m_backup  = nullptr;
    RestoreTab*   m_restore = nullptr;
    FilterTab*    m_filter  = nullptr;

     ProgressDialog* m_progressDlg = nullptr;
 
     // 内嵌进度栏（主窗口底部）
     QLabel*       m_progressStatus = nullptr;
     QProgressBar* m_progressInline = nullptr;

     std::unique_ptr<BackendAdapter> m_backend;
     SessionHistory m_sessionHistory;
     bool          m_isRestore = false;
     QStringList   m_processedFiles;
    QString       m_currentSource;
    QString       m_currentDest;
};

} // namespace pbackup::ui
 #include <QStringList>
