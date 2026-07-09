#include "LogPanel.h"
#include "Theme.h"

#include <QDateTime>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>

namespace pbackup::ui {

LogPanel::LogPanel(QWidget* parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    setMaximumBlockCount(5000);
    setFont(Theme::logFont());
    setMinimumHeight(160);
    setMaximumHeight(300);
}

void LogPanel::clearLog() { clear(); }

QString LogPanel::levelTag(Level lvl) const {
    switch (lvl) {
    case Info:    return QStringLiteral("INFO");
    case Warn:    return QStringLiteral("WARN");
    case Error:   return QStringLiteral("ERROR");
    case Success: return QStringLiteral("OK");
    }
    return QStringLiteral("INFO");
}

QString LogPanel::levelColor(Level lvl) const {
    switch (lvl) {
    case Info:    return QStringLiteral("#60A5FA");
    case Warn:    return QStringLiteral("#FBBF24");
    case Error:   return QStringLiteral("#212121");
    case Success: return QStringLiteral("#34D399");
    }
    return Theme::textColor();
}

void LogPanel::appendLog(Level lvl, const QString& text) {
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
    const QString line = QStringLiteral("[%1] [%2] %3")
        .arg(ts, levelTag(lvl), text);

    QTextCharFormat fmt;
    fmt.setForeground(QColor(levelColor(lvl)));
    QTextCursor c = textCursor();
    c.movePosition(QTextCursor::End);
    c.insertText(line + QStringLiteral("\n"), fmt);

    QScrollBar* v = verticalScrollBar();
    v->setValue(v->maximum());
}

} // namespace pbackup::ui