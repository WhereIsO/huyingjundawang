// BackupTab.h — 备份页：选源目录 / 选输出包 / 压缩·加密 / 开始·取消
#pragma once
#include <QWidget>
#include "BackendAdapter.h"

class QCheckBox;
class QLineEdit;
class QPushButton;

namespace pbackup::ui {

class PathPicker;

class BackupTab : public QWidget {
    Q_OBJECT
public:
    explicit BackupTab(QWidget* parent = nullptr);

    BackupRequest buildRequest() const;   // 从界面收集参数

signals:
    void startRequested();                // 用户点“开始备份”
    void cancelRequested();               // 用户点“取消”

public slots:
    void setBusy(bool busy);              // 运行时禁用输入、切换按钮

private slots:
    void onEncryptToggled(bool on);

private:
    PathPicker*  m_source   = nullptr;
    PathPicker*  m_output   = nullptr;
    QCheckBox*   m_compress = nullptr;
    QCheckBox*   m_encrypt  = nullptr;
    QLineEdit*   m_password = nullptr;
    QPushButton* m_start    = nullptr;
    QPushButton* m_cancel   = nullptr;
};

} // namespace pbackup::ui
