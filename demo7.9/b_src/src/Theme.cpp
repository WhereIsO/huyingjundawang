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
    QFont f(QStringLiteral("Consolas"));
    f.setStyleHint(QFont::Monospace);
    f.setPointSize(fontSize());
    return f;
}

QSize Theme::buttonSize()   { return QSize(96, 32); }
QSize Theme::lineEditSize() { return QSize(320, 28); }
QSize Theme::comboBoxSize() { return QSize(160, 28); }
int    Theme::rowSpacing()     { return 8; }
int    Theme::sectionSpacing() { return 14; }

// ---- Colors ----
QString Theme::bgColor()        { return QStringLiteral("#F0F2F5"); }
QString Theme::cardColor()      { return QStringLiteral("#FFFFFF"); }
QString Theme::cardBorder()     { return QStringLiteral("#E0E4E8"); }
QString Theme::primaryColor()   { return QStringLiteral("#1976D2"); }
QString Theme::primaryDark()    { return QStringLiteral("#1565C0"); }
QString Theme::primaryLight()   { return QStringLiteral("#BBDEFB"); }
QString Theme::accentColor()    { return QStringLiteral("#212121"); }
QString Theme::successColor()   { return QStringLiteral("#212121"); }
QString Theme::dangerColor()    { return QStringLiteral("#D32F2F"); }
QString Theme::textColor()      { return QStringLiteral("#212121"); }
QString Theme::mutedText()      { return QStringLiteral("#757575"); }
QString Theme::borderColor()    { return QStringLiteral("#D0D4D8"); }
QString Theme::focusColor()     { return QStringLiteral("#1976D2"); }
QString Theme::pageBg()          { return QStringLiteral("#F0F2F5"); }
QString Theme::secondaryText()   { return QStringLiteral("#616161"); }
QString Theme::accentLight()     { return QStringLiteral("#FFE0B2"); }
QString Theme::successLight()    { return QStringLiteral("#C8E6C9"); }
QString Theme::dangerLight()     { return QStringLiteral("#FFCDD2"); }
QString Theme::warningColor()    { return QStringLiteral("#212121"); }
QString Theme::warningLight()    { return QStringLiteral("#FFE0B2"); }
QString Theme::shadowColor()     { return QStringLiteral("#0000001A"); }
QString Theme::headerBg()        { return QStringLiteral("#1976D2"); }
QString Theme::headerText()      { return QStringLiteral("#FFFFFF"); }

QFont Theme::subtitleFont() {
    QFont f = appFont();
    f.setPointSize(fontSize() + 2);
    f.setBold(true);
    return f;
}

QSize Theme::largeButtonSize() { return QSize(120, 40); }
int   Theme::cardRadius()      { return 8; }
QString Theme::cardShadow()    { return QString(); }

// ---- Global Stylesheet ----
QString Theme::appStyleSheet() {
    const QString bg    = bgColor();
    const QString card  = cardColor();
    const QString cbor  = cardBorder();
    const QString pri   = primaryColor();
    const QString pdk   = primaryDark();
    const QString pli   = primaryLight();
    const QString acc   = accentColor();
    const QString suc   = successColor();
    const QString dng   = dangerColor();
    const QString txt   = textColor();
    const QString mtx   = mutedText();
    const QString bor   = borderColor();
    const QString foc   = focusColor();

    return QStringLiteral(
        // ----- Main Window -----
        "QMainWindow { background: %1; }"

        // ----- QTabWidget / QTabBar -----
        "QTabWidget::pane {"
        "  border: 1px solid %3;"
        "  border-top: none;"
        "  background: %2;"
        "  border-radius: 0 0 8px 8px;"
        "  top: -1px;"
        "}"
        "QTabBar::tab {"
        "  background: #E8EAF0;"
        "  color: %10;"
        "  padding: 10px 20px;"
        "  margin-right: 2px;"
        "  border: 1px solid %3;"
        "  border-bottom: none;"
        "  border-radius: 6px 6px 0 0;"
        "  font-size: 13px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: %2;"
        "  color: %4;"
        "  font-weight: bold;"
        "  border-bottom: 2px solid %4;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background: #F0F2F8;"
        "}"
        "QTabBar::tab:disabled {"
        "  color: %11;"
        "}"

        // ----- QPushButton -----
        "QPushButton {"
        "  background: %2;"
        "  color: %9;"
        "  border: 1px solid %11;"
        "  border-radius: 6px;"
        "  padding: 6px 16px;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: #F5F5F5;"
        "  border-color: #B0B4B8;"
        "}"
        "QPushButton:pressed {"
        "  background: #E8E8E8;"
        "}"
        "QPushButton:disabled {"
        "  color: %11;"
        "  background: #F5F5F5;"
        "  border-color: %11;"
        "}"

        // ----- Primary action button -----
        "QPushButton[class=\"primary\"] {"
        "  background: %4;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 10px 28px;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
        "QPushButton[class=\"primary\"]:hover {"
        "  background: %5;"
        "}"
        "QPushButton[class=\"primary\"]:pressed {"
        "  background: %5;"
        "}"
        "QPushButton[class=\"primary\"]:disabled {"
        "  background: #B0BEC5;"
        "  color: #FFFFFF;"
        "}"

        // ----- Danger / cancel button -----
        "QPushButton[class=\"danger\"] {"
        "  background: %8;"
        "  color: #FFFFFF;"
        "  border: 1px solid %8;"
        "  border-radius: 8px;"
        "  padding: 10px 28px;"
        "  font-size: 14px;"
        "}"
        "QPushButton[class=\"danger\"]:hover {"
        "  background: #E53935;"
        "  color: #FFFFFF;"
        "}"
        "QPushButton[class=\"danger\"]:disabled {"
        "  color: #FFFFFF;"
        "  border-color: #EF9A9A;"
        "  background: #EF9A9A;"
        "}"


        // ----- QLineEdit -----
        "QLineEdit {"
        "  background: %2;"
        "  color: %9;"
        "  border: 1px solid %11;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
       "  font-size: 13px;"
       "  selection-background-color: %6;"
        "  placeholder-text-color: #9E9E9E;"
       "}"
       "QLineEdit:focus {"
        "  border-color: %4;"
        "  border-width: 2px;"
        "  padding: 5px 9px;"
        "}"
        "QLineEdit:disabled {"
        "  background: #F5F5F5;"
        "  color: %11;"
        "}"

        // ----- QProgressBar -----
        "QProgressBar {"
        "  background: #E0E4E8;"
        "  border: none;"
        "  border-radius: 6px;"
        "  text-align: center;"
        "  font-size: 12px;"
        "  color: %9;"
        "  min-height: 20px;"
        "}"
        "QProgressBar::chunk {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 %4, stop:1 #42A5F5);"
        "  border-radius: 6px;"
        "}"

        // ----- QCheckBox -----
        "QCheckBox {"
        "  spacing: 8px;"
        "  font-size: 13px;"
        "  color: %9;"
        "}"
        "QCheckBox::indicator {"
        "  width: 18px;"
        "  height: 18px;"
        "  border: 1px solid %11;"
        "  border-radius: 4px;"
        "  background: %2;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background: %4;"
        "  border-color: %4;"
        "}"
        "QCheckBox::indicator:hover {"
        "  border-color: #909498;"
        "}"
        "QCheckBox:disabled {"
        "  color: %11;"
        "}"
        "QCheckBox::indicator:disabled {"
        "  background: #F0F0F0;"
        "  border-color: %11;"
        "}"

        // ----- QGroupBox -----
        "QGroupBox {"
        "  background: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 8px;"
        "  margin-top: 12px;"
        "  padding: 16px 16px 16px 16px;"
        "  font-size: 13px;"
        "  color: %9;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 14px;"
        "  padding: 0 6px;"
        "  color: %9;"
        "  font-weight: bold;"
        "}"

        // ----- QSplitter -----
        "QSplitter::handle {"
        "  background: %1;"
        "  height: 4px;"
        "}"

        // ----- QScrollArea / QScrollBar -----
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 8px;"
        "  margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #C0C4C8;"
        "  border-radius: 4px;"
        "  min-height: 30px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #A0A4A8;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: transparent;"
        "}"

        // ----- QDateEdit -----
        "QDateEdit {"
        "  background: %2;"
        "  color: %9;"
        "  border: 1px solid %11;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  font-size: 13px;"
        "}"
        "QDateEdit:focus {"
        "  border-color: %4;"
        "  border-width: 2px;"
        "}"
        "QDateEdit:disabled {"
        "  background: #F5F5F5;"
        "  color: %11;"
        "}"

        // ----- QLabel -----
        "QLabel {"
        "  color: %9;"
        "}"

        // ----- QPlainTextEdit (LogPanel) -----
        "QPlainTextEdit {"
        "  background: #FAFBFC;"
        "  color: %9;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 6px;"
        "  font-size: 12px;"
        "}"

    ).arg(bg, card, cbor, pri, pdk, pli, acc, dng, txt, mtx, bor, foc);
    //        %1   %2    %3    %4   %5   %6   %7   %8   %9   %10  %11  %12
}

} // namespace pbackup::ui


