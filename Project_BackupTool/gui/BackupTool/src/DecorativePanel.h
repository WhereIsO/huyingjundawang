#pragma once

#include <QColor>
#include <QFrame>
#include <QString>
#include <QStringList>

namespace pbackup::ui {

class DecorativePanel : public QFrame {
    Q_OBJECT
public:
    DecorativePanel(const QString& title,
                    const QString& subtitle,
                    const QStringList& steps,
                    const QColor& accent,
                    QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_accent;
};

} // namespace pbackup::ui
