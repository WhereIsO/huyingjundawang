#include "RestoreTab.h"
#include "PathPicker.h"
#include "Theme.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

namespace pbackup::ui {

RestoreTab::RestoreTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(Theme::sectionSpacing());

    // 标题
    auto* title = new QLabel(QStringLiteral("还原备份"), this);
    title->setFont(Theme::subtitleFont());
    title->setStyleSheet(QStringLiteral("color: %1; margin-bottom: 4px;").arg(Theme::textColor()));
    root->addWidget(title);

    // 备份包 + 目标目录
    m_pkg = new PathPicker(QStringLiteral("备份包"), PathPicker::OpenFile, this);
    m_pkg->setPlaceholder(QStringLiteral("选择 *.pbackup 备份包"));
    root->addWidget(m_pkg);

    m_dest = new PathPicker(QStringLiteral("目标目录"), PathPicker::Directory, this);
    m_dest->setPlaceholder(QStringLiteral("选择还原到的文件夹"));
    root->addWidget(m_dest);

    // 分隔线
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color: %1; max-height: 1px;").arg(Theme::cardBorder()));
    root->addWidget(sep);

    // 密码行
    auto* pwdRow = new QHBoxLayout();
    auto* pwdLabel = new QLabel(QStringLiteral("密码"), this);
    pwdLabel->setFont(Theme::appFont());
    pwdLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::mutedText()));
    pwdLabel->setMinimumWidth(56);
    pwdLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setFont(Theme::appFont());
    m_password->setMinimumHeight(Theme::lineEditSize().height());
    m_password->setPlaceholderText(QStringLiteral("若备份包已加密则填写"));
    pwdRow->addWidget(pwdLabel);
    pwdRow->addWidget(m_password);
    root->addLayout(pwdRow);

    // 覆盖选项
    m_overwrite = new QCheckBox(QStringLiteral("覆盖已存在的文件"), this);
    m_overwrite->setFont(Theme::appFont());
    root->addWidget(m_overwrite);

    root->addStretch();

    // 操作按钮
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_cancel = new QPushButton(QStringLiteral("取消"), this);
    m_cancel->setFont(Theme::appFont());
    m_cancel->setMinimumSize(96, 36);
    m_cancel->setProperty("class", QStringLiteral("danger"));
    m_cancel->setEnabled(false);
    m_start = new QPushButton(QStringLiteral("开始还原"), this);
    m_start->setFont(Theme::appFont());
    m_start->setMinimumSize(120, 40);
    m_start->setProperty("class", QStringLiteral("primary"));
    btnRow->addWidget(m_cancel);
    btnRow->addSpacing(12);
    btnRow->addWidget(m_start);
    root->addLayout(btnRow);

    connect(m_start,  &QPushButton::clicked, this, &RestoreTab::startRequested);
    connect(m_cancel, &QPushButton::clicked, this, &RestoreTab::cancelRequested);
}

RestoreRequest RestoreTab::buildRequest() const {
    RestoreRequest r;
    r.pkg       = m_pkg->text().trimmed().toStdString();
    r.destDir   = m_dest->text().trimmed().toStdString();
    r.password  = m_password->text().toStdString();
    r.overwrite = m_overwrite->isChecked();
    return r;
}

void RestoreTab::setBusy(bool busy) {
    m_pkg->setEnabled(!busy);
    m_dest->setEnabled(!busy);
    m_password->setEnabled(!busy);
    m_overwrite->setEnabled(!busy);
    m_start->setEnabled(!busy);
    m_cancel->setEnabled(busy);
}

} // namespace pbackup::ui