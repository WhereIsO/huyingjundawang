// main.cpp — 程序入口
// 说明：本 GUI 可独立运行（内置演示后端 Mock），无需等待后端交付。
// 接入真实后端后，设置环境变量 BACKUP_BACKEND_MODE=real 即切换。
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QFont>

int main(int argc, char* argv[]) {
    // 高 DPI 缩放，照顾大字号与高分屏（Qt 5.6+）。
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
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
