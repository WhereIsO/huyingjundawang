#include "FilterTab.h"
#include "Theme.h"

#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QDate>

namespace pbackup::ui {

// 统一构造带提示的输入框
static QLineEdit* makeEdit(QWidget* parent, const QString& hint) {
    auto* e = new QLineEdit(parent);
    e->setFont(Theme::appFont());
    e->setMinimumHeight(Theme::lineEditSize().height());
    e->setPlaceholderText(hint);
    return e;
}

FilterTab::FilterTab(QWidget* parent) : QWidget(parent) {
    // 外层：仅承载一个滚动区，保证窗口变小时过滤项可滚动、不被裁切
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    scroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(Theme::rowSpacing());

    // 标题
    auto* title = new QLabel(QStringLiteral("过滤条件（可留空 = 不过滤）"), content);
    title->setFont(Theme::subtitleFont());
    root->addWidget(title);

    // —— 路径 ——
    auto* gPath = new QGroupBox(QStringLiteral("① 路径"), content);
    gPath->setFont(Theme::appFont());
    auto* fPath = new QFormLayout(gPath);
    fPath->setSpacing(Theme::rowSpacing());
    m_includePath = makeEdit(content, QStringLiteral("仅包含含此子串的路径，多个用英文逗号隔开"));
    m_excludePath = makeEdit(content, QStringLiteral("排除含此子串的路径，多个用英文逗号隔开"));
    fPath->addRow(QStringLiteral("包含"), m_includePath);
    fPath->addRow(QStringLiteral("排除"), m_excludePath);
    root->addWidget(gPath);

    // —— 名称 ——
    auto* gName = new QGroupBox(QStringLiteral("② 名称"), content);
    gName->setFont(Theme::appFont());
    auto* fName = new QFormLayout(gName);
    m_nameGlob = makeEdit(content, QStringLiteral("通配符，例：*.docx；多个用英文逗号隔开"));
    fName->addRow(QStringLiteral("文件名"), m_nameGlob);
    root->addWidget(gName);

    // —— 类型 ——
    auto* gType = new QGroupBox(QStringLiteral("③ 类型"), content);
    gType->setFont(Theme::appFont());
    auto* hType = new QHBoxLayout(gType);
    hType->setSpacing(Theme::sectionSpacing());
    m_typeFile     = new QCheckBox(QStringLiteral("普通文件"), content);
    m_typeDir      = new QCheckBox(QStringLiteral("目录"), content);
    m_typeSymlink  = new QCheckBox(QStringLiteral("符号链接"), content);
    m_typeHardlink = new QCheckBox(QStringLiteral("硬链接"), content);
    for (QCheckBox* c : {m_typeFile, m_typeDir, m_typeSymlink, m_typeHardlink}) {
        c->setFont(Theme::appFont());
        c->setChecked(true);   // 默认全选
        hType->addWidget(c);
    }
    hType->addStretch();
    root->addWidget(gType);

    // —— 大小（字节） ——
    auto* gSize = new QGroupBox(QStringLiteral("④ 大小（字节）"), content);
    gSize->setFont(Theme::appFont());
    auto* hSize = new QHBoxLayout(gSize);
    m_sizeMin = makeEdit(content, QStringLiteral("最小，例：0"));
    m_sizeMax = makeEdit(content, QStringLiteral("最大，留空 = 不限"));
    hSize->addWidget(new QLabel(QStringLiteral("从"), content));
    hSize->addWidget(m_sizeMin);
    hSize->addWidget(new QLabel(QStringLiteral("到"), content));
    hSize->addWidget(m_sizeMax);
    hSize->addStretch();
    root->addWidget(gSize);

    // —— 修改时间 ——
    auto* gTime = new QGroupBox(QStringLiteral("⑤ 修改时间"), content);
    gTime->setFont(Theme::appFont());
    auto* hTime = new QHBoxLayout(gTime);
    m_useMtime = new QCheckBox(QStringLiteral("按修改时间过滤"), content);
    m_useMtime->setFont(Theme::appFont());
    m_mtimeAfter  = new QDateEdit(QDate::currentDate().addYears(-1), content);
    m_mtimeBefore = new QDateEdit(QDate::currentDate(), content);
    for (QDateEdit* d : {m_mtimeAfter, m_mtimeBefore}) {
        d->setFont(Theme::appFont());
        d->setCalendarPopup(true);
        d->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        d->setMinimumHeight(Theme::lineEditSize().height());
        d->setEnabled(false);
    }
    hTime->addWidget(m_useMtime);
    hTime->addWidget(new QLabel(QStringLiteral("从"), content));
    hTime->addWidget(m_mtimeAfter);
    hTime->addWidget(new QLabel(QStringLiteral("到"), content));
    hTime->addWidget(m_mtimeBefore);
    hTime->addStretch();
    connect(m_useMtime, &QCheckBox::toggled, m_mtimeAfter,  &QWidget::setEnabled);
    connect(m_useMtime, &QCheckBox::toggled, m_mtimeBefore, &QWidget::setEnabled);
    root->addWidget(gTime);

    // —— 所有者 ——
    auto* gUser = new QGroupBox(QStringLiteral("⑥ 所有者"), content);
    gUser->setFont(Theme::appFont());
    auto* fUser = new QFormLayout(gUser);
    m_ownerSid = makeEdit(content, QStringLiteral("Windows SID，留空 = 任意用户"));
    fUser->addRow(QStringLiteral("SID"), m_ownerSid);
    root->addWidget(gUser);

    root->addStretch();

    // 强制过滤页所有非按钮文字为黑色
    content->setStyleSheet(QStringLiteral(
        "QLabel { color: #000000; }"
        "QCheckBox { color: #000000; }"
        "QGroupBox { color: #000000; }"
        "QGroupBox::title { color: #000000; }"
        "QLineEdit { color: #000000; }"
        "QDateEdit { color: #000000; }"
        "QCheckBox::indicator { color: unset; }"
    ));

}

FilterSpec FilterTab::buildSpec() const {
    FilterSpec s;

    auto splitCsv = [](const QString& t) -> std::vector<std::string> {
        std::vector<std::string> out;
        for (const QString& part : t.split(QChar(','), Qt::SkipEmptyParts))
            out.push_back(part.trimmed().toStdString());
        return out;
    };

    s.includePath = splitCsv(m_includePath->text());
    s.excludePath = splitCsv(m_excludePath->text());
    s.nameGlob    = m_nameGlob->text().trimmed().toStdString();

    QStringList qtypes;
    if (m_typeFile->isChecked())     qtypes << QStringLiteral("file");
    if (m_typeDir->isChecked())      qtypes << QStringLiteral("dir");
    if (m_typeSymlink->isChecked())  qtypes << QStringLiteral("symlink");
    if (m_typeHardlink->isChecked()) qtypes << QStringLiteral("hardlink");
    s.typeFilter = qtypes.join(QChar(',')).toStdString();

    s.sizeMin = m_sizeMin->text().trimmed().toStdString();
    s.sizeMax = m_sizeMax->text().trimmed().toStdString();

    if (m_useMtime->isChecked()) {
        s.mtimeAfter  = m_mtimeAfter->date().toString(QStringLiteral("yyyy-MM-dd")).toStdString();
        s.mtimeBefore = m_mtimeBefore->date().toString(QStringLiteral("yyyy-MM-dd")).toStdString();
    }

    s.ownerSid = m_ownerSid->text().trimmed().toStdString();
    return s;
}

void FilterTab::setBusy(bool busy) {
    setEnabled(!busy);
}

} // namespace pbackup::ui
