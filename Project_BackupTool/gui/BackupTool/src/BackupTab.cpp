#include "BackupTab.h"
#include "DecorativePanel.h"
#include "PathPicker.h"
#include "Theme.h"

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace pbackup::ui {

namespace {

QFrame* makePanel(const QString& title, const QString& hint, QWidget* parent) {
    auto* panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("ContentPanel"));
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(Theme::rowSpacing());

    auto* titleLabel = new QLabel(title, panel);
    titleLabel->setObjectName(QStringLiteral("SectionTitle"));
    titleLabel->setFont(Theme::titleFont());
    layout->addWidget(titleLabel);

    auto* hintLabel = new QLabel(hint, panel);
    hintLabel->setObjectName(QStringLiteral("SectionHint"));
    hintLabel->setFont(Theme::appFont());
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);
    return panel;
}

} // namespace

BackupTab::BackupTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("PageRoot"));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(22, 22, 22, 22);
    root->setSpacing(Theme::sectionSpacing());

    auto* decor = new DecorativePanel(
        QStringLiteral("创建备份"),
        QStringLiteral("把目录树整理为单个 .pbackup 文件，可压缩、可加密、可按条件筛选。"),
        {QStringLiteral("选择源目录"),
         QStringLiteral("设置输出备份包"),
         QStringLiteral("确认压缩和加密策略"),
         QStringLiteral("启动后台备份任务")},
        QColor(Theme::primaryColor()),
        this);
    root->addWidget(decor, 0);

    auto* content = new QVBoxLayout();
    content->setSpacing(Theme::sectionSpacing());
    root->addLayout(content, 1);

    auto* pathPanel = makePanel(QStringLiteral("路径设置"),
                                QStringLiteral("源目录会被扫描为目录树，输出路径建议使用 .pbackup 后缀。"),
                                this);
    auto* pathLayout = qobject_cast<QVBoxLayout*>(pathPanel->layout());
    m_source = new PathPicker(QStringLiteral("源目录"), PathPicker::Directory, this);
    m_source->setPlaceholder(QStringLiteral("选择要备份的文件夹"));
    pathLayout->addWidget(m_source);

    m_output = new PathPicker(QStringLiteral("备份包"), PathPicker::SaveFile, this);
    m_output->setPlaceholder(QStringLiteral("保存为 *.pbackup"));
    pathLayout->addWidget(m_output);
    content->addWidget(pathPanel);

    auto* optionPanel = new QFrame(this);
    optionPanel->setObjectName(QStringLiteral("OptionPanel"));
    auto* optionLayout = new QVBoxLayout(optionPanel);
    optionLayout->setContentsMargins(18, 16, 18, 16);
    optionLayout->setSpacing(Theme::rowSpacing());

    auto* optionTitle = new QLabel(QStringLiteral("备份策略"), optionPanel);
    optionTitle->setObjectName(QStringLiteral("SectionTitle"));
    optionTitle->setFont(Theme::titleFont());
    optionLayout->addWidget(optionTitle);

    auto* optionHint = new QLabel(QStringLiteral("压缩适合文本和重复数据；加密会使用 AES-256-GCM 保护备份包。"), optionPanel);
    optionHint->setObjectName(QStringLiteral("SectionHint"));
    optionHint->setFont(Theme::appFont());
    optionHint->setWordWrap(true);
    optionLayout->addWidget(optionHint);

    auto* band = new QFrame(optionPanel);
    band->setObjectName(QStringLiteral("InlineBand"));
    auto* bandLayout = new QVBoxLayout(band);
    bandLayout->setContentsMargins(14, 12, 14, 12);
    bandLayout->setSpacing(Theme::rowSpacing());

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
    bandLayout->addLayout(optRow);

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
    bandLayout->addLayout(pwdRow);
    optionLayout->addWidget(band);
    content->addWidget(optionPanel);

    auto* actionPanel = new QFrame(this);
    actionPanel->setObjectName(QStringLiteral("ActionPanel"));
    auto* actionLayout = new QHBoxLayout(actionPanel);
    actionLayout->setContentsMargins(18, 14, 18, 14);
    actionLayout->setSpacing(Theme::rowSpacing());
    auto* actionHint = new QLabel(QStringLiteral("任务运行时会锁定输入，并在底部日志中显示进度。"), actionPanel);
    actionHint->setObjectName(QStringLiteral("SectionHint"));
    actionHint->setFont(Theme::appFont());
    actionHint->setWordWrap(true);
    actionLayout->addWidget(actionHint, 1);
    auto* btnRow = new QHBoxLayout();
    m_cancel = new QPushButton(QStringLiteral("取消"), this);
    m_cancel->setObjectName(QStringLiteral("SecondaryButton"));
    m_cancel->setFont(Theme::appFont());
    m_cancel->setMinimumSize(Theme::buttonSize());
    m_cancel->setEnabled(false);
    m_start = new QPushButton(QStringLiteral("开始备份"), this);
    m_start->setObjectName(QStringLiteral("PrimaryButton"));
    m_start->setFont(Theme::appFont());
    m_start->setMinimumSize(Theme::buttonSize());
    btnRow->addWidget(m_cancel);
    btnRow->addWidget(m_start);
    actionLayout->addLayout(btnRow);
    content->addWidget(actionPanel);
    content->addStretch(1);

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
