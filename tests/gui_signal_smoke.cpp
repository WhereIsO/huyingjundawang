#include "MainWindow.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <QMetaObject>
#include <QProgressBar>

Q_DECLARE_METATYPE(pbackup::ui::Progress)

TEST(GuiSignalSmokeTest, ProgressSlotUpdatesInlineWidgets) {
    pbackup::ui::MainWindow window;

    auto* progress = window.findChild<QProgressBar*>();
    ASSERT_NE(progress, nullptr);

    pbackup::ui::Progress p;
    p.totalBytes = 100;
    p.doneBytes = 40;
    p.totalFiles = 2;
    p.doneFiles = 1;
    p.currentFile = "a.txt";
    p.stage = "Writing";

    ASSERT_TRUE(QMetaObject::invokeMethod(&window, "onProgress",
                                          Qt::DirectConnection,
                                          Q_ARG(pbackup::ui::Progress, p)));
    EXPECT_EQ(progress->value(), 40);

    bool foundStatus = false;
    for (auto* label : window.findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("Writing")) &&
            label->text().contains(QStringLiteral("1/2"))) {
            foundStatus = true;
            break;
        }
    }
    EXPECT_TRUE(foundStatus);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    qRegisterMetaType<pbackup::ui::Progress>("Progress");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
