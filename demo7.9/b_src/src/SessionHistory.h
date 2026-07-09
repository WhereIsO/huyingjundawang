#pragma once
#include <QString>
#include <QStringList>
#include <QList>

namespace pbackup::ui {

/// 单次操作的记录条目
struct HistoryEntry {
    QString timestamp;   // HH:mm:ss
    QString operation;   // "备份" / "还原"
    QString source;      // 源路径或包路径
    QString dest;        // 目标路径
    QString status;      // "成功" / "失败"
    int     fileCount;   // 处理文件数
     QString summary;     // 完成摘要文本
     QStringList files;   // 具体处理过的文件路径
};

/// 会话历史管理器：记录本次会话所有操作，持久化到临时文件，关闭时清理
class SessionHistory {
public:
    SessionHistory();
    ~SessionHistory();

    void addEntry(const HistoryEntry& entry);
    void clear();

    /// 返回格式化好的纯文本行，供日志面板展示
    QStringList formatAll() const;

    int entryCount() const { return m_entries.size(); }
    const HistoryEntry& entry(int i) const { return m_entries[i]; }
    const QList<HistoryEntry>& entries() const { return m_entries; }

private:
    static QString tempFilePath();
    void saveToFile();
    void loadFromFile();
    void cleanupFile();

    QList<HistoryEntry> m_entries;
};

} // namespace pbackup::ui
