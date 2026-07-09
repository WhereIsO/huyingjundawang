#include "FilterTab.h"
#include "DecorativePanel.h"
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
#include <array>

namespace pbackup::ui {

// 统一构造大字号输入框
static QLineEdit* makeEdit(QWidget* parent, const QString& hint) {
    auto* e = new QLineEdit(parent);
    e->setFont(Theme::appFont());
    e->setMinimumHeight(Theme::lineEditSize().height());
    e->setPlaceholderText(hint);
    return e;
}

static QGroupBox* makeGroup(const QString& title, QWidget* parent) {
    auto* g = new QGroupBox(title, parent);
    g->setFont(Theme::appFont());
    return g;
}

FilterTab::FilterTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("PageRoot"));
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(22, 22, 22, 22);
    outer->setSpacing(Theme::sectionSpacing());

    auto* decor = new DecorativePanel(
        QStringLiteral("筛选规则"),
        QStringLiteral("默认不过滤。开启某类规则后，多个条件按 AND 关系共同生效。"),
        {QStringLiteral("路径包含或排除"),
         QStringLiteral("按名称通配符匹配"),
         QStringLiteral("按类型、大小、时间筛选"),
         QStringLiteral("可选按 Owner SID 限定")},
        QColor(Theme::accentColor()),
        this);
    outer->addWidget(decor, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll, 1);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(Theme::sectionSpacing());

    auto* title = new QLabel(QStringLiteral("规则编辑"), content);
    title->setFont(Theme::titleFont());
    title->setObjectName(QStringLiteral("SectionTitle"));
    root->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("留空表示不限制。路径支持多个英文逗号分隔项，名称通配符支持单个模式。"), content);
    hint->setObjectName(QStringLiteral("SectionHint"));
    hint->setFont(Theme::appFont());
    hint->setWordWrap(true);
    root->addWidget(hint);

    // —— 路径 ——
    auto* gPath = makeGroup(QStringLiteral("① 路径"), this);
    auto* fPath = new QFormLayout(gPath);
    fPath->setSpacing(Theme::rowSpacing());
    m_includePath = makeEdit(this, QStringLiteral("仅包含含此子串的路径，多个用英文逗号隔开"));
    m_excludePath = makeEdit(this, QStringLiteral("排除含此子串的路径，多个用英文逗号隔开"));
    fPath->addRow(QStringLiteral("包含"), m_includePath);
    fPath->addRow(QStringLiteral("排除"), m_excludePath);
    root->addWidget(gPath);

    // —— 名称 ——
    auto* gName = makeGroup(QStringLiteral("② 名称"), this);
    auto* fName = new QFormLayout(gName);
    m_nameGlob = makeEdit(this, QStringLiteral("通配符，例：*.docx"));
    fName->addRow(QStringLiteral("文件名"), m_nameGlob);
    root->addWidget(gName);

    // —— 类型 ——
    auto* gType = makeGroup(QStringLiteral("③ 类型"), this);
    auto* typeRoot = new QVBoxLayout(gType);
    typeRoot->setSpacing(Theme::rowSpacing());
    m_useTypeFilter = new QCheckBox(QStringLiteral("按类型筛选"), this);
    m_useTypeFilter->setFont(Theme::appFont());
    typeRoot->addWidget(m_useTypeFilter);

    auto* hType = new QHBoxLayout();
    hType->setSpacing(Theme::rowSpacing());
    m_typeFile     = new QCheckBox(QStringLiteral("普通文件"), this);
    m_typeDir      = new QCheckBox(QStringLiteral("目录"), this);
    m_typeEmptyDir = new QCheckBox(QStringLiteral("空目录"), this);
    m_typeSymlink  = new QCheckBox(QStringLiteral("符号链接"), this);
    m_typeHardlink = new QCheckBox(QStringLiteral("硬链接"), this);
    m_typeJunction = new QCheckBox(QStringLiteral("Junction"), this);
    m_typeReparse  = new QCheckBox(QStringLiteral("ReparsePoint"), this);
    const std::array<QCheckBox*, 7> typeChecks = {
        m_typeFile, m_typeDir, m_typeEmptyDir, m_typeSymlink,
        m_typeHardlink, m_typeJunction, m_typeReparse
    };
    for (QCheckBox* c : typeChecks) {
        c->setFont(Theme::appFont());
        c->setChecked(true);
        c->setEnabled(false);
        hType->addWidget(c);
    }
    hType->addStretch();
    typeRoot->addLayout(hType);
    connect(m_useTypeFilter, &QCheckBox::toggled, this, [typeChecks](bool enabled) {
        for (QCheckBox* c : typeChecks) c->setEnabled(enabled);
    });
    root->addWidget(gType);

    // —— 大小 ——
    auto* gSize = makeGroup(QStringLiteral("④ 大小（字节）"), this);
    auto* hSize = new QHBoxLayout(gSize);
    m_sizeMin = makeEdit(this, QStringLiteral("最小，例：0"));
    m_sizeMax = makeEdit(this, QStringLiteral("最大，留空 = 不限"));
    hSize->addWidget(new QLabel(QStringLiteral("从"), this));
    hSize->addWidget(m_sizeMin);
    hSize->addWidget(new QLabel(QStringLiteral("到"), this));
    hSize->addWidget(m_sizeMax);
    root->addWidget(gSize);

    // —— 时间 ——
    auto* gTime = makeGroup(QStringLiteral("⑤ 修改时间"), this);
    auto* hTime = new QHBoxLayout(gTime);
    m_useMtime = new QCheckBox(QStringLiteral("按修改时间筛选"), this);
    m_useMtime->setFont(Theme::appFont());
    m_mtimeAfter  = new QDateEdit(QDate::currentDate().addYears(-1), this);
    m_mtimeBefore = new QDateEdit(QDate::currentDate(), this);
    for (QDateEdit* d : {m_mtimeAfter, m_mtimeBefore}) {
        d->setFont(Theme::appFont());
        d->setCalendarPopup(true);
        d->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        d->setMinimumHeight(Theme::lineEditSize().height());
        d->setEnabled(false);
    }
    hTime->addWidget(m_useMtime);
    hTime->addWidget(new QLabel(QStringLiteral("从"), this));
    hTime->addWidget(m_mtimeAfter);
    hTime->addWidget(new QLabel(QStringLiteral("到"), this));
    hTime->addWidget(m_mtimeBefore);
    hTime->addStretch();
    connect(m_useMtime, &QCheckBox::toggled, m_mtimeAfter,  &QWidget::setEnabled);
    connect(m_useMtime, &QCheckBox::toggled, m_mtimeBefore, &QWidget::setEnabled);
    root->addWidget(gTime);

    // —— 用户 ——
    auto* gUser = makeGroup(QStringLiteral("⑥ 所有者"), this);
    auto* fUser = new QFormLayout(gUser);
    m_ownerSid = makeEdit(this, QStringLiteral("Windows SID，留空 = 任意用户"));
    fUser->addRow(QStringLiteral("SID"), m_ownerSid);
    root->addWidget(gUser);

    root->addStretch();
}

FilterSpec FilterTab::buildSpec() const {
    FilterSpec s;

    auto splitCsv = [](const QString& t) -> QStringList {
        QStringList out;
        for (const QString& part : t.split(QChar(','), Qt::SkipEmptyParts))
            out << part.trimmed();
        return out;
    };

    s.includePath = splitCsv(m_includePath->text());
    s.excludePath = splitCsv(m_excludePath->text());
    s.nameGlob    = m_nameGlob->text().trimmed();

    if (m_useTypeFilter->isChecked()) {
        QStringList types;
        if (m_typeFile->isChecked())     types << QStringLiteral("file");
        if (m_typeDir->isChecked())      types << QStringLiteral("dir");
        if (m_typeEmptyDir->isChecked()) types << QStringLiteral("emptydir");
        if (m_typeSymlink->isChecked())  types << QStringLiteral("symlink");
        if (m_typeHardlink->isChecked()) types << QStringLiteral("hardlink");
        if (m_typeJunction->isChecked()) types << QStringLiteral("junction");
        if (m_typeReparse->isChecked())  types << QStringLiteral("reparse");
        s.typeFilter = types.join(QChar(','));
    }

    s.sizeMin = m_sizeMin->text().trimmed();
    s.sizeMax = m_sizeMax->text().trimmed();

    if (m_useMtime->isChecked()) {
        s.mtimeAfter  = m_mtimeAfter->date().toString(QStringLiteral("yyyy-MM-dd"));
        s.mtimeBefore = m_mtimeBefore->date().toString(QStringLiteral("yyyy-MM-dd"));
    }

    s.ownerSid = m_ownerSid->text().trimmed();
    return s;
}

void FilterTab::setBusy(bool busy) {
    setEnabled(!busy);
}

} // namespace pbackup::ui
