// ProgressDialog.h — 独立进度窗口：进度条 + 状态 + 日志
#pragma once
 #include <QDialog>
 #include <QString>
 #include <QStringList>
 #include <QList>
 #include "SessionHistory.h"

 class QPushButton;
 class QTreeWidget;

namespace pbackup::ui {

class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(QWidget* parent = nullptr);

     /// 展示历史记录（树形折叠）
     void showHistory(const QList<HistoryEntry>& entries);
 
 public slots:

private:
     QTreeWidget*  m_tree     = nullptr;
     QPushButton*  m_closeBtn = nullptr;
};

} // namespace pbackup::ui
