#include "Theme.h"
#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <cstdlib>

namespace pbackup::ui {

int Theme::fontSize() {
    if (qEnvironmentVariableIsSet("BACKUP_FONT_SIZE")) {
        bool ok = false;
        int v = QString::fromLocal8Bit(qgetenv("BACKUP_FONT_SIZE")).toInt(&ok);
        if (ok && v >= 8 && v <= 20) return v;
    }
#ifdef APP_DEFAULT_FONT_SIZE
    return APP_DEFAULT_FONT_SIZE;
#else
    return 10;
#endif
}

QFont Theme::appFont() {
    QFont f = QGuiApplication::font();
    f.setPointSize(fontSize());
    f.setFamily(QStringLiteral("Microsoft YaHei UI"));
    return f;
}

QFont Theme::titleFont() {
    QFont f = appFont();
    f.setPointSize(fontSize() + 4);
    f.setBold(true);
    return f;
}

QFont Theme::logFont() {
    QFont f("Consolas");
    f.setStyleHint(QFont::Monospace);
    f.setPointSize(fontSize());
    return f;
}

QSize Theme::buttonSize()   { return QSize(96, 32); }
QSize Theme::lineEditSize() { return QSize(320, 28); }
QSize Theme::comboBoxSize() { return QSize(160, 28); }
int    Theme::rowSpacing()     { return 8; }
int    Theme::sectionSpacing() { return 14; }

QString Theme::primaryColor()    { return QStringLiteral("#1565C0"); }
QString Theme::accentColor()     { return QStringLiteral("#FF6F00"); }
QString Theme::dangerColor()     { return QStringLiteral("#C62828"); }
QString Theme::surfaceColor()    { return QStringLiteral("#FAFAFA"); }
QString Theme::surfaceAltColor() { return QStringLiteral("#FFFFFF"); }
QString Theme::textColor()       { return QStringLiteral("#212121"); }
QString Theme::mutedTextColor()  { return QStringLiteral("#616161"); }
QString Theme::borderColor()     { return QStringLiteral("#BDBDBD"); }

} // namespace pbackup::ui
