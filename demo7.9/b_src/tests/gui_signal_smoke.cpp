#include "LogPanel.h"
#include "MainWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QMetaObject>
#include <QProgressBar>

TEST(GuiSignalSmokeTest, ProgressAndLogSlotsUpdateWidgets) {
    qputenv("BACKUP_BACKEND_MODE", "real");
    pbackup::ui::MainWindow window;

    auto* progress = window.findChild<QProgressBar*>();
    auto* logPanel = window.findChild<pbackup::ui::LogPanel*>();
    ASSERT_NE(progress, nullptr);
    ASSERT_NE(logPanel, nullptr);

    pbackup::ui::Progress p;
    p.totalBytes = 100;
    p.doneBytes = 40;
    p.totalFiles = 2;
    p.doneFiles = 1;
    p.currentFile = QStringLiteral("a.txt");
    p.stage = QStringLiteral("Writing");

    ASSERT_TRUE(QMetaObject::invokeMethod(&window, "onProgress",
                                          Qt::DirectConnection,
                                          Q_ARG(pbackup::ui::Progress, p)));
    EXPECT_EQ(progress->value(), 40);

    ASSERT_TRUE(QMetaObject::invokeMethod(&window, "onLog",
                                          Qt::DirectConnection,
                                          Q_ARG(int, 3),
                                          Q_ARG(QString, QStringLiteral("GUI 日志刷新测试"))));
    EXPECT_TRUE(logPanel->toPlainText().contains(QStringLiteral("GUI 日志刷新测试")));
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    qRegisterMetaType<pbackup::ui::Progress>("Progress");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
