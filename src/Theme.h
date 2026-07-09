// Theme.h — Modern styling system: colors, fonts, and global stylesheet
#pragma once

#include <QString>
#include <QFont>
#include <QSize>
#include <QColor>

namespace pbackup::ui {

struct Theme {
    // ---- Font ----
    static int    fontSize();
    static QFont  appFont();
    static QFont  titleFont();
    static QFont  subtitleFont();
    static QFont  logFont();

    // ---- Dimensions ----
    static QSize  buttonSize();
    static QSize  largeButtonSize();
    static QSize  lineEditSize();
    static QSize  comboBoxSize();
    static int    rowSpacing();
    static int    sectionSpacing();
    static int    cardRadius();

    // ---- Colors ----
    static QString pageBg();
    static QString bgColor();
    static QString cardColor();
    static QString cardBorder();
    static QString primaryColor();
    static QString primaryDark();
    static QString primaryLight();
    static QString accentColor();
    static QString accentLight();
    static QString successColor();
    static QString successLight();
    static QString dangerColor();
    static QString dangerLight();
    static QString warningColor();
    static QString warningLight();
    static QString textColor();
    static QString secondaryText();
    static QString mutedText();
    static QString borderColor();
    static QString focusColor();
    static QString shadowColor();
    static QString headerBg();
    static QString headerText();

    // ---- Global Stylesheet ----
    static QString appStyleSheet();

    // ---- Helper: CSS shadow ----
    static QString cardShadow();
};

} // namespace pbackup::ui
