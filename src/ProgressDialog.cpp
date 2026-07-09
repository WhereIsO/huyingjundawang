// ProgressDialog.cpp - 历史任务查看窗口（树形折叠）
#include "ProgressDialog.h"
#include "Theme.h"

#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QColor>
#include <QFont>

namespace pbackup::ui {

ProgressDialog::ProgressDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("历史任务"));
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
    setFixedSize(680, 520);
    setObjectName(QStringLiteral("progressDialog"));

    setStyleSheet(Theme::appStyleSheet() + QStringLiteral(
        "QDialog#progressDialog { background: %1; }"
    ).arg(Theme::pageBg()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("历史任务"), this);
    title->setFont(Theme::subtitleFont());
    title->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::textColor()));
    root->addWidget(title);

    // 树形折叠列表
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->setIndentation(24);
    m_tree->setFont(Theme::logFont());
    m_tree->setMinimumHeight(360);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: %1; border: 1px solid %2; border-radius: 4px; color: %3; }"
        "QTreeWidget::item { padding: 5px 8px; }"
        "QTreeWidget::item:hover { background: %4; }"
    ).arg(Theme::cardColor(), Theme::cardBorder(), Theme::textColor(), Theme::accentLight()));
    root->addWidget(m_tree, 1);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    m_closeBtn->setMinimumSize(80, 32);
    m_closeBtn->setFont(Theme::appFont());
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    btnRow->addWidget(m_closeBtn);
    root->addLayout(btnRow);

    if (parent) {
        QRect parentGeo = parent->geometry();
        QScreen* scr = parent->screen();
        QRect screenGeo = scr ? scr->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
        move(parentGeo.center().x() - width() / 2,
             parentGeo.center().y() - height() / 2);
    }
}

void ProgressDialog::showHistory(const QList<HistoryEntry>& entries) {
    m_tree->clear();

    auto makeItem = [](QTreeWidgetItem* parent, const QString& text, const QColor& color) {
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, text);
        item->setForeground(0, color);
        return item;
    };

    // 汇总标题
    auto* header = makeItem(m_tree->invisibleRootItem(),
        QStringLiteral("\u2501\u2501\u2501 \u672c\u6b21\u4f1a\u8bdd\u5171 %1 \u6b21\u64cd\u4f5c\uff0c\u70b9\u51fb\u5c55\u5f00\u67e5\u770b\u8be6\u60c5 \u2501\u2501\u2501").arg(entries.size()),
        QColor(Theme::secondaryText()));
    header->setFlags(header->flags() & ~Qt::ItemIsSelectable);

    for (const auto& e : entries) {
        QColor statusColor = (e.status == QStringLiteral("\u6210\u529f"))
            ? QColor(Theme::successColor())
            : (e.status == QStringLiteral("\u5931\u8d25"))
            ? QColor(Theme::dangerColor())
            : QColor(Theme::textColor());

        auto* item = makeItem(m_tree->invisibleRootItem(),
            QStringLiteral("[%1] %2  |  %3  |  %4 \u4e2a\u6587\u4ef6")
                .arg(e.timestamp, e.operation, e.status)
                .arg(e.fileCount),
            statusColor);
        QFont boldFont = item->font(0);
        boldFont.setBold(true);
        item->setFont(0, boldFont);
        item->setExpanded(false);

        // 源/目标信息
        if (!e.source.isEmpty() && !e.dest.isEmpty()) {
            makeItem(item, QStringLiteral("  \u6e90\u76ee\u5f55: %1").arg(e.source),
                QColor(Theme::secondaryText()));
            makeItem(item, QStringLiteral("  \u76ee\u6807: %1").arg(e.dest),
                QColor(Theme::secondaryText()));
        }

        // 失败原因
        if (e.status == QStringLiteral("\u5931\u8d25") && !e.summary.isEmpty()) {
            makeItem(item, QStringLiteral("  \u539f\u56e0: %1").arg(e.summary),
                QColor(Theme::warningColor()));
        }

        // 文件列表
        if (!e.files.isEmpty()) {
            auto* fileHeader = makeItem(item,
                QStringLiteral("  \u5904\u7406\u7684\u6587\u4ef6 (%1 \u4e2a):").arg(e.files.size()),
                QColor(Theme::secondaryText()));
            QFont fhFont = fileHeader->font(0);
            fhFont.setItalic(true);
            fileHeader->setFont(0, fhFont);

            for (const auto& f : e.files) {
                makeItem(item, QStringLiteral("    \u2022 %1").arg(f),
                    QColor(Theme::secondaryText()));
            }
        }
    }

    m_tree->resizeColumnToContents(0);
}

} // namespace pbackup::ui

