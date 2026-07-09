#include "PathPicker.h"
#include "Theme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace pbackup::ui {

PathPicker::PathPicker(const QString& label, Kind kind, QWidget* parent)
    : QWidget(parent), m_kind(kind) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_label = new QLabel(label, this);
    m_label->setMinimumWidth(56);
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setFont(Theme::appFont());
    m_label->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::mutedText()));

    m_edit = new QLineEdit(this);
    m_edit->setMinimumSize(Theme::lineEditSize());
    m_edit->setFont(Theme::appFont());
    connect(m_edit, &QLineEdit::textChanged, this, &PathPicker::onTextEdited);

    m_btn = new QPushButton(QStringLiteral("浏览"), this);
    m_btn->setMinimumSize(70, Theme::lineEditSize().height());
    m_btn->setFont(Theme::appFont());
    m_btn->setProperty("class", QStringLiteral("secondary"));
    connect(m_btn, &QPushButton::clicked, this, &PathPicker::onBrowse);

    layout->addWidget(m_label);
    layout->addWidget(m_edit, 1);
    layout->addWidget(m_btn);
}

QString PathPicker::text() const { return m_edit->text(); }

void PathPicker::setText(const QString& path) {
    m_edit->setText(path);
}

void PathPicker::setPlaceholder(const QString& hint) {
    m_edit->setPlaceholderText(hint);
}

void PathPicker::onBrowse() {
    QString chosen;
    switch (m_kind) {
    case Directory:
        chosen = QFileDialog::getExistingDirectory(this, m_label->text(), m_edit->text());
        break;
    case OpenFile:
        chosen = QFileDialog::getOpenFileName(this, m_label->text(), m_edit->text());
        break;
    case SaveFile:
        chosen = QFileDialog::getSaveFileName(this, m_label->text(), m_edit->text(),
                                              QStringLiteral("备份包(*.pbackup);;所有文件(*)"));
        break;
    }
    if (!chosen.isEmpty()) {
        m_edit->setText(chosen);
    }
}

void PathPicker::onTextEdited(const QString& t) {
    emit textChanged(t);
}

} // namespace pbackup::ui