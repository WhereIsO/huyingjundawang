// BackendAdapter.h — 抽象后端，GUI 不直接依赖 backup_core。
// 等 GPT 交付 src/core/ 之后，写真实实现替换 Mock 即可。
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <memory>

namespace pbackup::ui {

struct BackupRequest {
    QString sourceDir;
    QString outputPkg;
    bool    compress   = true;     // 哈夫曼
    bool    encrypt    = false;    // AES-256-GCM
    QString password;              // 明文密码；进入后端前会被零拷贝
};

struct RestoreRequest {
    QString pkg;
    QString destDir;
    QString password;
    bool    overwrite = false;
};

struct FilterSpec {
    // 6 类筛选：每类为空表示"不过滤"
    QStringList includePath;     // 路径包含子串
    QStringList excludePath;
    QString     nameGlob;        // 例: *.docx
    QString     typeFilter;      // 逗号分隔：file,dir,symlink,hardlink
    QString     sizeMin;         // 字节
    QString     sizeMax;
    QString     mtimeAfter;      // yyyy-MM-dd
    QString     mtimeBefore;
    QString     ownerSid;        // 留空 = 任何
};

struct Progress {
    quint64 totalBytes  = 0;
    quint64 doneBytes   = 0;
    quint32 totalFiles  = 0;
    quint32 doneFiles   = 0;
    QString currentFile;
    QString stage;               // Scanning / Filtering / Writing / Verifying / Done
    QStringList warnings;
    int     percent() const {
        if (totalBytes == 0) return 0;
        return int(double(doneBytes) * 100.0 / double(totalBytes));
    }
};

class BackendAdapter : public QObject {
    Q_OBJECT
public:
    explicit BackendAdapter(QObject* parent = nullptr);
    virtual ~BackendAdapter() = default;

    // 返回 true 表示成功开始；后续通过 progress/failed/finished 信号异步回调。
    virtual bool startBackup(const BackupRequest& req, const FilterSpec& filter) = 0;
    virtual bool startRestore(const RestoreRequest& req) = 0;
    virtual void cancel() = 0;
    virtual bool isRunning() const = 0;

signals:
    void progress(const Progress& p);
    void log(int level, const QString& text);  // 0=Info 1=Warn 2=Error 3=Ok
    void finished(bool success, const QString& summary);
    void failed(const QString& reason);
};

// Mock 实现：在 GPT 后端到位前，用于自洽演示与录制视频。
class MockBackend : public BackendAdapter {
    Q_OBJECT
public:
    explicit MockBackend(QObject* parent = nullptr);
    bool startBackup(const BackupRequest& req, const FilterSpec& filter) override;
    bool startRestore(const RestoreRequest& req) override;
    void cancel() override;
    bool isRunning() const override { return m_running.load(); }

private:
    std::atomic_bool m_running{false};
    std::atomic_bool m_cancel{false};
    void runMockBackup(const BackupRequest& req, const FilterSpec& filter);
    void runMockRestore(const RestoreRequest& req);
};

// 工厂：当前返回 Mock；后端到位后改成读取环境变量 BACKUP_BACKEND_MODE=real。
std::unique_ptr<BackendAdapter> createBackend(QObject* parent = nullptr);

} // namespace pbackup::ui

// 让 Progress 能作为排队连接（跨线程）信号参数传递。
Q_DECLARE_METATYPE(pbackup::ui::Progress)
