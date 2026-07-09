// main.cpp — 程序入口
// 说明：本 GUI 默认使用真实后端（RealBackend），支持完整备份/还原。
// 若只需界面演示（Mock），设置 BACKUP_BACKEND_MODE=mock。
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QFont>

int main(int argc, char* argv[]) {
    // 高 DPI 缩放，照顾大字号与高分屏（Qt 5.6+）。
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("BackupTool"));
    app.setOrganizationName(QStringLiteral("BackupTool"));

    // 全局大字号（可用环境变量 BACKUP_FONT_SIZE 覆盖）。
    app.setFont(pbackup::ui::Theme::appFont());

    pbackup::ui::MainWindow w;
    w.show();
    return app.exec();
}
