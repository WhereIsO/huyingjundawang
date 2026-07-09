#include "SessionHistory.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QtGlobal>

namespace pbackup::ui {

SessionHistory::SessionHistory() {
    loadFromFile();
}

SessionHistory::~SessionHistory() {
    // 析构时清理临时文件
    cleanupFile();
}

QString SessionHistory::tempFilePath() {
    // 在可执行文件同级建 data/ 目录存放会话记录
    QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/session_history.txt");
}

void SessionHistory::addEntry(const HistoryEntry& entry) {
    m_entries.append(entry);
    saveToFile();
}

void SessionHistory::clear() {
    m_entries.clear();
    cleanupFile();
}

void SessionHistory::loadFromFile() {
    QFile file(tempFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        // 格式: timestamp|operation|source|dest|status|fileCount|summary
        QStringList parts = line.split(QLatin1Char('|'));
        if (parts.size() < 7)
            continue;
        HistoryEntry e;
        e.timestamp  = parts[0];
        e.operation  = parts[1];
        e.source     = parts[2];
        e.dest       = parts[3];
        e.status     = parts[4];
        e.fileCount  = parts[5].toInt();
        e.summary    = parts[6];
        if (parts.size() >= 8 && !parts[7].trimmed().isEmpty()) {
            e.files = parts[7].split(QChar(0x01));
        }
        m_entries.append(e);
    }
    file.close();
}

void SessionHistory::saveToFile() {
    QFile file(tempFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    for (const auto& e : m_entries) {
        out << e.timestamp  << QLatin1Char('|')
            << e.operation  << QLatin1Char('|')
            << e.source     << QLatin1Char('|')
            << e.dest       << QLatin1Char('|')
            << e.status     << QLatin1Char('|')
            << e.fileCount  << QLatin1Char('|')
            << e.summary    << QLatin1Char('|')
            << e.files.join(QChar(0x01)) << QStringLiteral("\n");
    }
    file.close();
}

void SessionHistory::cleanupFile() {
    QFile::remove(tempFilePath());
}

QStringList SessionHistory::formatAll() const {
    QStringList lines;
    lines << QStringLiteral("━━━ 本次会话共 %1 次操作 ━━━").arg(m_entries.size());
    lines << QString();

    for (int i = 0; i < m_entries.size(); ++i) {
        const auto& e = m_entries[i];
        lines << QStringLiteral("[%1] %2  |  %3  |  %4 个文件")
            .arg(e.timestamp, e.operation, e.status)
            .arg(e.fileCount);
        if (!e.source.isEmpty() && !e.dest.isEmpty()) {
            lines << QStringLiteral("     %1").arg(e.source);
            lines << QStringLiteral("     →  %1").arg(e.dest);
        }
        if (e.status == QStringLiteral("失败") && !e.summary.isEmpty()) {
            lines << QStringLiteral("     原因: %1").arg(e.summary);
        }
        if (i < m_entries.size() - 1)
            lines << QString();
    }
    return lines;
}

} // namespace pbackup::ui
