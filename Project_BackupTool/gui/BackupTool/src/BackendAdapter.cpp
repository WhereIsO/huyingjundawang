#include "BackendAdapter.h"
#include "MockBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QTimer>
#include <QtConcurrent>

namespace pbackup::ui {

BackendAdapter::BackendAdapter(QObject* parent) : QObject(parent) {}

std::unique_ptr<BackendAdapter> createRealBackend(QObject* parent);

// ------------------------------------------------------------------
// MockBackend
// ------------------------------------------------------------------
MockBackend::MockBackend(QObject* parent) : BackendAdapter(parent) {}

static quint64 dirSize(const QString& path) {
    quint64 total = 0;
    QDir d(path);
    if (!d.exists()) return 0;
    const QFileInfoList entries =
        d.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& fi : entries) {
        if (fi.isDir()) total += dirSize(fi.absoluteFilePath());
        else            total += fi.size();
    }
    return total;
}

static quint32 countFiles(const QString& path) {
    quint32 total = 0;
    QDir d(path);
    if (!d.exists()) return 0;
    const QFileInfoList entries =
        d.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& fi : entries) {
        if (fi.isDir()) total += countFiles(fi.absoluteFilePath());
        else            total += 1;
    }
    return total;
}

bool MockBackend::startBackup(const BackupRequest& req, const FilterSpec& filter) {
    if (m_running.load()) return false;
    m_running = true; m_cancel = false;
    (void)filter;  // Mock 暂不应用筛选
    QtConcurrent::run([this, req]() { runMockBackup(req, FilterSpec{}); });
    return true;
}

bool MockBackend::startRestore(const RestoreRequest& req) {
    if (m_running.load()) return false;
    m_running = true; m_cancel = false;
    QtConcurrent::run([this, req]() { runMockRestore(req); });
    return true;
}

void MockBackend::cancel() { m_cancel = true; }

void MockBackend::runMockBackup(const BackupRequest& req, const FilterSpec&) {
    emit log(0, QStringLiteral("开始备份：%1 → %2").arg(req.sourceDir, req.outputPkg));
    if (!QDir(req.sourceDir).exists()) {
        emit failed(QStringLiteral("源目录不存在：%1").arg(req.sourceDir));
        m_running = false;
        return;
    }
    QDir().mkpath(QFileInfo(req.outputPkg).absolutePath());

    Progress p;
    p.stage     = QStringLiteral("Scanning");
    p.totalBytes = dirSize(req.sourceDir);
    p.totalFiles = countFiles(req.sourceDir);
    emit progress(p);

    if (p.totalBytes == 0) p.totalBytes = 1;  // 避免除零

    const int totalSteps = 100;
    for (int i = 1; i <= totalSteps; ++i) {
        if (m_cancel.load()) { emit failed(QStringLiteral("已取消")); m_running = false; return; }
        QThread::msleep(30);
        p.doneBytes = quint64(double(p.totalBytes) * i / totalSteps);
        p.doneFiles = quint32(double(p.totalFiles) * i / totalSteps);
        p.currentFile = QStringLiteral("src/%1.dat").arg(i, 4, 10, QChar('0'));
        p.stage = (i < 30) ? QStringLiteral("Scanning")
                  : (i < 50) ? QStringLiteral("Filtering")
                  : (i < 95) ? QStringLiteral("Writing")
                  :            QStringLiteral("Verifying");
        emit progress(p);
    }

    // 写一个空文件以模拟落盘
    QFile f(req.outputPkg);
    if (f.open(QIODevice::WriteOnly)) {
        f.write("PBACKUP\x01\x00\x00\x00MOCK");
        f.close();
    }
    emit log(3, QStringLiteral("备份完成：%1（Mock）").arg(req.outputPkg));
    emit finished(true, QStringLiteral("Mock 备份完成，共处理 %1 个文件").arg(p.totalFiles));
    m_running = false;
}

void MockBackend::runMockRestore(const RestoreRequest& req) {
    emit log(0, QStringLiteral("开始还原：%1 → %2").arg(req.pkg, req.destDir));
    if (!QFile::exists(req.pkg)) {
        emit failed(QStringLiteral("备份包不存在：%1").arg(req.pkg));
        m_running = false;
        return;
    }
    QDir().mkpath(req.destDir);
    Progress p; p.stage = QStringLiteral("Restoring"); p.totalBytes = 100; p.totalFiles = 1;
    for (int i = 1; i <= 100; ++i) {
        if (m_cancel.load()) { emit failed(QStringLiteral("已取消")); m_running = false; return; }
        QThread::msleep(20);
        p.doneBytes = i; p.doneFiles = (i == 100 ? 1u : 0u);
        p.currentFile = QStringLiteral("dst/restored_%1.dat").arg(i, 3, 10, QChar('0'));
        emit progress(p);
    }
    emit log(3, QStringLiteral("还原完成（Mock）"));
    emit finished(true, QStringLiteral("Mock 还原完成"));
    m_running = false;
}

// ------------------------------------------------------------------
// Factory
// ------------------------------------------------------------------
std::unique_ptr<BackendAdapter> createBackend(QObject* parent) {
    const QByteArray mode = qgetenv("BACKUP_BACKEND_MODE").toLower();
    if (mode == "real") {
        return createRealBackend(parent);
    }
    return std::make_unique<MockBackend>(parent);
}

} // namespace pbackup::ui
