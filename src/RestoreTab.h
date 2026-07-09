// RestoreTab.h — 还原页：选备份包 / 选目标目录 / 密码 / 覆盖 / 开始·取消
#pragma once
#include <QWidget>
#include "BackendAdapter.h"

class QCheckBox;
class QLineEdit;
class QPushButton;

namespace pbackup::ui {

class PathPicker;

class RestoreTab : public QWidget {
    Q_OBJECT
public:
    explicit RestoreTab(QWidget* parent = nullptr);

    RestoreRequest buildRequest() const;   // 从界面收集参数

signals:
    void startRequested();                 // 用户点“开始还原”
    void cancelRequested();                // 用户点“取消”

public slots:
    void setBusy(bool busy);               // 运行时禁用输入、切换按钮

private:
    PathPicker*  m_pkg      = nullptr;
    PathPicker*  m_dest     = nullptr;
    QLineEdit*   m_password = nullptr;
    QCheckBox*   m_overwrite = nullptr;
    QPushButton* m_start    = nullptr;
    QPushButton* m_cancel   = nullptr;
};

} // namespace pbackup::ui
