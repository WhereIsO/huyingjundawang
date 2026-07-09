#include "RestoreTab.h"
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

QFrame* makeRestorePanel(const QString& title, const QString& hint, QWidget* parent) {
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

RestoreTab::RestoreTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("PageRoot"));
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(22, 22, 22, 22);
    root->setSpacing(Theme::sectionSpacing());

    auto* decor = new DecorativePanel(
        QStringLiteral("还原备份"),
        QStringLiteral("读取 .pbackup 包，校验完整性后恢复目录结构、文件内容和可恢复元数据。"),
        {QStringLiteral("选择备份包"),
         QStringLiteral("指定还原目标"),
         QStringLiteral("输入密码或覆盖策略"),
         QStringLiteral("恢复文件并写入日志")},
        QColor(Theme::successColor()),
        this);
    root->addWidget(decor, 0);

    auto* content = new QVBoxLayout();
    content->setSpacing(Theme::sectionSpacing());
    root->addLayout(content, 1);

    auto* filePanel = makeRestorePanel(QStringLiteral("还原来源"),
                                       QStringLiteral("选择由本工具生成的 .pbackup 文件，读取前会做格式和完整性校验。"),
                                       this);
    auto* fileLayout = qobject_cast<QVBoxLayout*>(filePanel->layout());
    m_pkg = new PathPicker(QStringLiteral("备份包"), PathPicker::OpenFile, this);
    m_pkg->setPlaceholder(QStringLiteral("选择 *.pbackup 备份包"));
    fileLayout->addWidget(m_pkg);

    m_dest = new PathPicker(QStringLiteral("目标目录"), PathPicker::Directory, this);
    m_dest->setPlaceholder(QStringLiteral("选择还原到的文件夹"));
    fileLayout->addWidget(m_dest);
    content->addWidget(filePanel);

    auto* optionPanel = new QFrame(this);
    optionPanel->setObjectName(QStringLiteral("OptionPanel"));
    auto* optionLayout = new QVBoxLayout(optionPanel);
    optionLayout->setContentsMargins(18, 16, 18, 16);
    optionLayout->setSpacing(Theme::rowSpacing());

    auto* optionTitle = new QLabel(QStringLiteral("安全与冲突处理"), optionPanel);
    optionTitle->setObjectName(QStringLiteral("SectionTitle"));
    optionTitle->setFont(Theme::titleFont());
    optionLayout->addWidget(optionTitle);

    auto* optionHint = new QLabel(QStringLiteral("加密包必须输入正确密码；覆盖选项用于处理目标目录里的同名文件。"), optionPanel);
    optionHint->setObjectName(QStringLiteral("SectionHint"));
    optionHint->setFont(Theme::appFont());
    optionHint->setWordWrap(true);
    optionLayout->addWidget(optionHint);

    auto* band = new QFrame(optionPanel);
    band->setObjectName(QStringLiteral("InlineBand"));
    auto* bandLayout = new QVBoxLayout(band);
    bandLayout->setContentsMargins(14, 12, 14, 12);
    bandLayout->setSpacing(Theme::rowSpacing());

    auto* pwdRow = new QHBoxLayout();
    auto* pwdLabel = new QLabel(QStringLiteral("密码"), this);
    pwdLabel->setFont(Theme::appFont());
    pwdLabel->setMinimumWidth(72);
    pwdLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setFont(Theme::appFont());
    m_password->setMinimumHeight(Theme::lineEditSize().height());
    m_password->setPlaceholderText(QStringLiteral("若备份包已加密则填写"));
    pwdRow->addWidget(pwdLabel);
    pwdRow->addWidget(m_password);
    bandLayout->addLayout(pwdRow);

    m_overwrite = new QCheckBox(QStringLiteral("覆盖已存在的文件"), this);
    m_overwrite->setFont(Theme::appFont());
    bandLayout->addWidget(m_overwrite);
    optionLayout->addWidget(band);
    content->addWidget(optionPanel);

    auto* actionPanel = new QFrame(this);
    actionPanel->setObjectName(QStringLiteral("ActionPanel"));
    auto* actionLayout = new QHBoxLayout(actionPanel);
    actionLayout->setContentsMargins(18, 14, 18, 14);
    actionLayout->setSpacing(Theme::rowSpacing());
    auto* actionHint = new QLabel(QStringLiteral("建议先还原到空目录，确认无误后再替换原始文件。"), actionPanel);
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
    m_start = new QPushButton(QStringLiteral("开始还原"), this);
    m_start->setObjectName(QStringLiteral("PrimaryButton"));
    m_start->setFont(Theme::appFont());
    m_start->setMinimumSize(Theme::buttonSize());
    btnRow->addWidget(m_cancel);
    btnRow->addWidget(m_start);
    actionLayout->addLayout(btnRow);
    content->addWidget(actionPanel);
    content->addStretch(1);

    connect(m_start,  &QPushButton::clicked, this, &RestoreTab::startRequested);
    connect(m_cancel, &QPushButton::clicked, this, &RestoreTab::cancelRequested);
}

RestoreRequest RestoreTab::buildRequest() const {
    RestoreRequest r;
    r.pkg       = m_pkg->text().trimmed();
    r.destDir   = m_dest->text().trimmed();
    r.password  = m_password->text();
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
