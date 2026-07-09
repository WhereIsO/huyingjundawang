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
        if (ok && v >= 8 && v <= 24) return v;
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

QSize Theme::buttonSize()   { return QSize(112, 36); }
QSize Theme::lineEditSize() { return QSize(320, 32); }
QSize Theme::comboBoxSize() { return QSize(160, 32); }
int    Theme::rowSpacing()     { return 10; }
int    Theme::sectionSpacing() { return 16; }

QString Theme::primaryColor()    { return QStringLiteral("#0F6B7A"); }
QString Theme::accentColor()     { return QStringLiteral("#D97706"); }
QString Theme::dangerColor()     { return QStringLiteral("#B42318"); }
QString Theme::surfaceColor()    { return QStringLiteral("#F8FAFC"); }
QString Theme::surfaceAltColor() { return QStringLiteral("#FFFFFF"); }
QString Theme::textColor()       { return QStringLiteral("#17202A"); }
QString Theme::mutedTextColor()  { return QStringLiteral("#5D6B78"); }
QString Theme::borderColor()     { return QStringLiteral("#CBD5E1"); }
QString Theme::successColor()    { return QStringLiteral("#1F7A4D"); }
QString Theme::windowBgColor()   { return QStringLiteral("#EDF3F5"); }

QString Theme::styleSheet() {
    return QStringLiteral(R"(
        QWidget {
            color: %1;
            selection-background-color: %2;
        }
        QMainWindow, QWidget#AppCentral {
            background: %3;
        }
        QWidget#PageRoot {
            background: #F2F6F7;
        }
        QFrame#DecorPanel, QFrame#ContentPanel, QFrame#OptionPanel, QFrame#ActionPanel {
            background: %5;
            border: 1px solid %4;
            border-radius: 8px;
        }
        QLabel#DecorTitle {
            color: %2;
            background: transparent;
        }
        QLabel#DecorSubtitle {
            color: %6;
            background: transparent;
            line-height: 130%;
        }
        QLabel#DecorStep {
            color: %1;
            background: rgba(255,255,255,178);
            border: 1px solid #DDE7EA;
            border-radius: 6px;
            padding: 8px 10px;
        }
        QLabel#SectionTitle {
            color: %1;
            font-weight: 700;
            background: transparent;
        }
        QLabel#SectionHint {
            color: %6;
            background: transparent;
        }
        QFrame#InlineBand {
            background: #F4F8F9;
            border: 1px solid #DCE7EA;
            border-radius: 8px;
        }
        QTabWidget::pane {
            border: 1px solid %4;
            background: %5;
            top: -1px;
        }
        QTabBar::tab {
            background: #E7EEF1;
            color: %6;
            border: 1px solid %4;
            border-bottom: none;
            padding: 8px 18px;
            min-width: 88px;
        }
        QTabBar::tab:selected {
            background: %5;
            color: %2;
            font-weight: 600;
        }
        QTabBar::tab:hover {
            background: #F3F7F8;
        }
        QGroupBox {
            background: %5;
            border: 1px solid %4;
            margin-top: 14px;
            padding: 12px 10px 10px 10px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: %2;
            background: %5;
        }
        QLineEdit, QDateEdit {
            background: %5;
            border: 1px solid %4;
            padding: 5px 8px;
        }
        QLineEdit:focus, QDateEdit:focus {
            border: 1px solid %2;
            background: #FBFEFF;
        }
        QPushButton {
            background: #F8FAFC;
            border: 1px solid %4;
            border-radius: 6px;
            padding: 6px 12px;
        }
        QPushButton:hover {
            background: #FFFFFF;
            border-color: %2;
        }
        QPushButton:pressed {
            background: #DDE7EA;
        }
        QPushButton:disabled {
            background: #E5E7EB;
            color: #8792A0;
            border-color: #D1D5DB;
        }
        QPushButton#PrimaryButton {
            background: %2;
            color: #FFFFFF;
            border: 1px solid %2;
            font-weight: 700;
            padding: 8px 18px;
        }
        QPushButton#PrimaryButton:hover {
            background: #0B5D6B;
            border-color: #0B5D6B;
        }
        QPushButton#PrimaryButton:pressed {
            background: #084D59;
        }
        QPushButton#SecondaryButton {
            background: #FFFFFF;
            color: %6;
            border: 1px solid %4;
            padding: 8px 18px;
        }
        QPushButton#SecondaryButton:hover {
            color: %2;
            border-color: %2;
        }
        QPushButton#BrowseButton {
            background: #EAF1F3;
            color: %2;
            border: 1px solid #C7D8DD;
            font-weight: 600;
        }
        QCheckBox {
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
        }
        QProgressBar {
            background: #DDE7EA;
            border: 1px solid %4;
            text-align: center;
            color: %1;
        }
        QProgressBar::chunk {
            background: %7;
        }
        QSplitter::handle {
            background: #DDE7EA;
        }
        QScrollArea {
            background: transparent;
            border: none;
        }
        QPlainTextEdit#LogPanel {
            background: #111827;
            color: #E5E7EB;
            border: 1px solid #233143;
            border-radius: 8px;
            padding: 8px;
        }
    )")
        .arg(textColor(),
             primaryColor(),
             windowBgColor(),
             borderColor(),
             surfaceAltColor(),
             mutedTextColor(),
             successColor());
}

} // namespace pbackup::ui
