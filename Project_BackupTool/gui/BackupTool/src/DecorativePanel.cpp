#include "DecorativePanel.h"

#include "Theme.h"

#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

namespace pbackup::ui {

DecorativePanel::DecorativePanel(const QString& title,
                                 const QString& subtitle,
                                 const QStringList& steps,
                                 const QColor& accent,
                                 QWidget* parent)
    : QFrame(parent), m_accent(accent) {
    setObjectName(QStringLiteral("DecorPanel"));
    setMinimumWidth(250);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 22, 22, 22);
    root->setSpacing(14);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("DecorTitle"));
    titleLabel->setFont(Theme::titleFont());
    titleLabel->setWordWrap(true);
    root->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(subtitle, this);
    subtitleLabel->setObjectName(QStringLiteral("DecorSubtitle"));
    subtitleLabel->setFont(Theme::appFont());
    subtitleLabel->setWordWrap(true);
    root->addWidget(subtitleLabel);

    root->addSpacing(10);
    for (int i = 0; i < steps.size(); ++i) {
        auto* step = new QLabel(QStringLiteral("%1  %2")
                                    .arg(i + 1, 2, 10, QChar('0'))
                                    .arg(steps.at(i)),
                                this);
        step->setObjectName(QStringLiteral("DecorStep"));
        step->setFont(Theme::appFont());
        step->setWordWrap(true);
        root->addWidget(step);
    }

    root->addStretch(1);
}

void DecorativePanel::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF box = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(QColor(Theme::borderColor()), 1));
    painter.setBrush(QColor("#F7FBFC"));
    painter.drawRoundedRect(box, 8, 8);

    QColor pale = m_accent;
    pale.setAlpha(24);
    QColor mid = m_accent;
    mid.setAlpha(42);

    painter.setPen(Qt::NoPen);
    painter.setBrush(pale);
    painter.drawEllipse(QRect(width() - 150, -55, 210, 210));
    painter.drawEllipse(QRect(-70, height() - 120, 170, 170));

    painter.setBrush(mid);
    for (int x = width() - 168; x < width() - 20; x += 32) {
        for (int y = height() - 126; y < height() - 20; y += 32) {
            painter.drawRect(QRect(x, y, 12, 12));
        }
    }

    painter.setPen(QPen(m_accent, 3));
    painter.drawLine(QPoint(24, height() - 44), QPoint(width() - 36, height() - 92));
    painter.setPen(QPen(QColor("#D7E3E7"), 1));
    painter.drawLine(QPoint(24, height() - 30), QPoint(width() - 74, height() - 66));

    Q_UNUSED(event);
}

} // namespace pbackup::ui
