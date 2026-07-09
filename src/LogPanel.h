// LogPanel.h — 实时日志面板，自动滚动、级别着色
#pragma once
#include <QPlainTextEdit>
#include <QString>

namespace pbackup::ui {

class LogPanel : public QPlainTextEdit {
    Q_OBJECT
public:
    enum Level { Info, Warn, Error, Success };
    Q_ENUM(Level)

    explicit LogPanel(QWidget* parent = nullptr);

public slots:
    void appendLog(Level lvl, const QString& text);
    void appendInfo(const QString& text)    { appendLog(Info, text); }
    void appendWarn(const QString& text)    { appendLog(Warn, text); }
    void appendError(const QString& text)   { appendLog(Error, text); }
    void appendSuccess(const QString& text) { appendLog(Success, text); }
    void clearLog();

private:
    QString levelTag(Level lvl) const;
    QString levelColor(Level lvl) const;
};

} // namespace pbackup::ui
