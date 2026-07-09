// Theme.h — 大字号 / 大按钮 / 高对比度 主题
// 依据用户偏好（记忆：GUI 做大 + 全自动），统一字号与控件尺寸。
#pragma once
#include <QFont>
#include <QSize>
#include <QString>

namespace pbackup::ui {

struct Theme {
    // 默认字号。环境变量 BACKUP_FONT_SIZE 可覆盖。
    static int fontSize();

    static QFont appFont();                       // 全局字体
    static QFont titleFont();                     // 页面标题
    static QFont logFont();                       // 日志面板等宽

    // 控件尺寸（按大字号缩放）。
    static QSize buttonSize();                    // 主按钮 240×64
    static QSize lineEditSize();                  // 输入框
    static QSize comboBoxSize();
    static int    rowSpacing();                   // 表单行距
    static int    sectionSpacing();               // 段落间距

    // 高对比度配色
    static QString primaryColor();                // #1565C0
    static QString accentColor();                 // #FF6F00
    static QString dangerColor();                 // #C62828
    static QString surfaceColor();
    static QString surfaceAltColor();
    static QString textColor();
    static QString mutedTextColor();
    static QString borderColor();
};

} // namespace pbackup::ui
