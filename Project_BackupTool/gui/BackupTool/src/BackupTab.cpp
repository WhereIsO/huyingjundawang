#include "BackupTab.h"
#include "PathPicker.h"
#include "Theme.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace pbackup::ui {

BackupTab::BackupTab(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(Theme::sectionSpacing());

    auto* title = new QLabel(QStringLiteral("创建备份"), this);
    title->setFont(Theme::titleFont());
    root->addWidget(title);

    m_source = new PathPicker(QStringLiteral("源目录"), PathPicker::Directory, this);
    m_source->setPlaceholder(QStringLiteral("选择要备份的文件夹"));
    root->addWidget(m_source);

    m_output = new PathPicker(QStringLiteral("备份包"), PathPicker::SaveFile, this);
    m_output->setPlaceholder(QStringLiteral("保存为 *.pbackup"));
    root->addWidget(m_output);

    // 选项行：压缩 / 加密
    auto* optRow = new QHBoxLayout();
    optRow->setSpacing(Theme::sectionSpacing());
    m_compress = new QCheckBox(QStringLiteral("哈夫曼压缩"), this);
    m_compress->setChecked(true);
    m_compress->setFont(Theme::appFont());
    m_encrypt = new QCheckBox(QStringLiteral("加密（AES-256）"), this);
    m_encrypt->setFont(Theme::appFont());
    optRow->addWidget(m_compress);
    optRow->addWidget(m_encrypt);
    optRow->addStretch();
    root->addLayout(optRow);

    // 密码行（勾选加密才启用）
    auto* pwdRow = new QHBoxLayout();
    auto* pwdLabel = new QLabel(QStringLiteral("密码"), this);
    pwdLabel->setFont(Theme::appFont());
    pwdLabel->setMinimumWidth(72);
    pwdLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setFont(Theme::appFont());
    m_password->setMinimumHeight(Theme::lineEditSize().height());
    m_password->setPlaceholderText(QStringLiteral("勾选加密后填写"));
    m_password->setEnabled(false);
    pwdRow->addWidget(pwdLabel);
    pwdRow->addWidget(m_password);
    root->addLayout(pwdRow);

    root->addStretch();

    // 操作按钮
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_cancel = new QPushButton(QStringLiteral("取消"), this);
    m_cancel->setFont(Theme::appFont());
    m_cancel->setMinimumSize(Theme::buttonSize());
    m_cancel->setEnabled(false);
    m_start = new QPushButton(QStringLiteral("开始备份"), this);
    m_start->setFont(Theme::appFont());
    m_start->setMinimumSize(Theme::buttonSize());
    m_start->setStyleSheet(QStringLiteral(
        "QPushButton{background:%1;color:#fff;border:none;border-radius:8px;}"
        "QPushButton:disabled{background:#B0BEC5;}")
        .arg(Theme::primaryColor()));
    btnRow->addWidget(m_cancel);
    btnRow->addWidget(m_start);
    root->addLayout(btnRow);

    connect(m_encrypt, &QCheckBox::toggled, this, &BackupTab::onEncryptToggled);
    connect(m_start,  &QPushButton::clicked, this, &BackupTab::startRequested);
    connect(m_cancel, &QPushButton::clicked, this, &BackupTab::cancelRequested);
}

void BackupTab::onEncryptToggled(bool on) {
    m_password->setEnabled(on);
    if (!on) m_password->clear();
}

BackupRequest BackupTab::buildRequest() const {
    BackupRequest r;
    r.sourceDir = m_source->text().trimmed();
    r.outputPkg = m_output->text().trimmed();
    r.compress  = m_compress->isChecked();
    r.encrypt   = m_encrypt->isChecked();
    r.password  = m_encrypt->isChecked() ? m_password->text() : QString();
    return r;
}

void BackupTab::setBusy(bool busy) {
    m_source->setEnabled(!busy);
    m_output->setEnabled(!busy);
    m_compress->setEnabled(!busy);
    m_encrypt->setEnabled(!busy);
    m_password->setEnabled(!busy && m_encrypt->isChecked());
    m_start->setEnabled(!busy);
    m_cancel->setEnabled(busy);
}

} // namespace pbackup::ui
