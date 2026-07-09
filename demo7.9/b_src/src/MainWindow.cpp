#include "MainWindow.h"
#include "BackupTab.h"
#include "RestoreTab.h"
#include "FilterTab.h"
#include "LogPanel.h"
#include "ProgressDialog.h"
#include "Theme.h"

#include <QTabWidget>
#include <QTabBar>
#include <QLabel>
 #include <QPushButton>
 #include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
 #include <QApplication>
 #include <QMessageBox>
 #include <QCloseEvent>
 #include <QDateTime>
 #include <QMetaObject>

namespace pbackup::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("数据备份工具"));
    resize(980, 680);
    setMinimumSize(800, 560);

    // 全局样式表
    static_cast<QApplication*>(qApp)->setStyleSheet(Theme::appStyleSheet());

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralWidget"));
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 头部栏 ----
    auto* header = new QWidget(central);
    header->setObjectName(QStringLiteral("headerBar"));
    header->setFixedHeight(52);
    header->setStyleSheet(QStringLiteral(
        "QWidget#headerBar { background: %1; border-bottom: 1px solid %2; }")
        .arg(Theme::headerBg(), Theme::cardBorder()));
    auto* hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(24, 0, 24, 0);

    auto* appIcon = new QLabel(QStringLiteral("  "), header);
    appIcon->setFixedSize(28, 28);
    appIcon->setStyleSheet(QStringLiteral(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "  stop:0 %1, stop:1 %2);"
        "border-radius: 6px;").arg(Theme::primaryColor(), Theme::primaryDark()));

    auto* appTitle = new QLabel(QStringLiteral("数据备份工具"), header);
    appTitle->setStyleSheet(QStringLiteral(
        "font-size: 15px; font-weight: 700; color: %1; padding-left: 10px;")
        .arg(Theme::headerText()));

    auto* appSub = new QLabel(QStringLiteral("v1.0"), header);
    appSub->setStyleSheet(QStringLiteral(
        "font-size: 11px; color: %1;").arg(Theme::mutedText()));

    hdrLayout->addWidget(appIcon);
    hdrLayout->addWidget(appTitle);
    hdrLayout->addWidget(appSub);
    hdrLayout->addStretch();

     // 历史任务按钮
     auto* historyBtn = new QPushButton(QStringLiteral("历史任务"), header);
     historyBtn->setProperty("class", QStringLiteral("secondary"));
     historyBtn->setMinimumSize(90, 30);
     historyBtn->setFont(Theme::appFont());
     hdrLayout->addWidget(historyBtn);
     m_progressDlg = new ProgressDialog(this);
     connect(historyBtn, &QPushButton::clicked, this, [this]() {
         m_progressDlg->showHistory(m_sessionHistory.entries());
         if (m_progressDlg->isVisible())
             m_progressDlg->hide();
         else {
             m_progressDlg->show();
             m_progressDlg->raise();
             m_progressDlg->activateWindow();
         }
     });

    root->addWidget(header);

    // ---- 内容区域 ----
    auto* content = new QWidget(central);
    content->setObjectName(QStringLiteral("contentArea"));
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 16, 20, 16);
    contentLayout->setSpacing(Theme::sectionSpacing());

    // 标签页
    m_tabs = new QTabWidget(content);
    m_tabs->setFont(Theme::appFont());
    m_backup  = new BackupTab(m_tabs);
    m_restore = new RestoreTab(m_tabs);
    m_filter  = new FilterTab(m_tabs);
    m_tabs->addTab(m_backup,  QStringLiteral("备份"));
    m_tabs->addTab(m_filter,  QStringLiteral("过滤"));
    m_tabs->addTab(m_restore, QStringLiteral("还原"));
    contentLayout->addWidget(m_tabs);

    // 底部区域
    // bottomWidget is created in the bottom section below

    // 截图/演示用：环境变量 BACKUP_UI_TAB=0/1/2 指定初始标签页
    {
        bool okTab = false;
        const int tabIdx = qgetenv("BACKUP_UI_TAB").toInt(&okTab);
        if (okTab && tabIdx >= 0 && tabIdx < m_tabs->count())
            m_tabs->setCurrentIndex(tabIdx);
    }

    contentLayout->addWidget(m_tabs, 1);
    root->addWidget(content, 1);

    // ---- 底部进度栏（常驻） ----
    auto* bottomBar = new QWidget(central);
    bottomBar->setObjectName("bottomBar");
    bottomBar->setFixedHeight(36);
    bottomBar->setStyleSheet(QStringLiteral(
        "QWidget#bottomBar { background: %1; border-top: 1px solid %2; }")
        .arg(Theme::headerBg(), Theme::cardBorder()));
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(16, 0, 16, 0);
    bottomLayout->setSpacing(8);

    m_progressStatus = new QLabel(QStringLiteral("就绪"), bottomBar);
    m_progressStatus->setFont(Theme::appFont());
    m_progressStatus->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 12px;").arg(Theme::secondaryText()));

    m_progressInline = new QProgressBar(bottomBar);
    m_progressInline->setRange(0, 100);
    m_progressInline->setValue(0);
    m_progressInline->setFixedSize(180, 18);
    m_progressInline->setFont(Theme::appFont());
    m_progressInline->setTextVisible(true);

    bottomLayout->addWidget(m_progressStatus, 1);
    bottomLayout->addWidget(m_progressInline);
    root->addWidget(bottomBar);

    setCentralWidget(central);

    // ---- 后端 ----
    m_backend = createBackend();
     // 设置回调（跨线程安全：通过 QMetaObject::invokeMethod 队列到主线程执行）
     m_backend->setOnProgress([this](const Progress& p) {
         QMetaObject::invokeMethod(this, [this, p]() {
             onProgress(p);
         }, Qt::QueuedConnection);
     });
     m_backend->setOnLog([this](int level, const std::string& text) {
         QMetaObject::invokeMethod(this, [this, level, text]() {
             onLog(level, QString::fromUtf8(text.data(), static_cast<int>(text.size())));
         }, Qt::QueuedConnection);
     });
     m_backend->setOnFinished([this](bool success, const std::string& summary) {
         QMetaObject::invokeMethod(this, [this, success, summary]() {
             onFinished(success, QString::fromUtf8(summary.data(), static_cast<int>(summary.size())));
         }, Qt::QueuedConnection);
     });
     m_backend->setOnFailed([this](const std::string& reason) {
         QMetaObject::invokeMethod(this, [this, reason]() {
             onFailed(QString::fromUtf8(reason.data(), static_cast<int>(reason.size())));
         }, Qt::QueuedConnection);
     });

    connect(m_backup,  &BackupTab::startRequested,  this, &MainWindow::onBackupStart);
    connect(m_backup,  &BackupTab::cancelRequested, this, &MainWindow::onCancel);
    connect(m_restore, &RestoreTab::startRequested, this, &MainWindow::onRestoreStart);
    connect(m_restore, &RestoreTab::cancelRequested,this, &MainWindow::onCancel);

     // 加载之前的会话记录（如果有）
     if (m_sessionHistory.entryCount() > 0) {
         m_progressDlg->showHistory(m_sessionHistory.entries());
     }
}


void MainWindow::onBackupStart() {
    const BackupRequest req = m_backup->buildRequest();
    if (req.sourceDir.empty() || req.outputPkg.empty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先选择源目录和备份包保存路径。"));
        return;
    }
    if (req.encrypt && req.password.empty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("已勾选加密，请填写密码。"));
        return;
    }
     const FilterSpec filter = m_filter->buildSpec();
     m_isRestore = false;
     setBusy(true);
    m_processedFiles.clear();
    m_currentSource = QString::fromStdString(req.sourceDir);
    m_currentDest   = QString::fromStdString(req.outputPkg);
     // 底部进度栏将在任务开始时自动显示进度
    if (!m_backend->startBackup(req, filter)) {
    // backend busy
        setBusy(false);
    }
}

void MainWindow::onRestoreStart() {
    const RestoreRequest req = m_restore->buildRequest();
    if (req.pkg.empty() || req.destDir.empty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先选择备份包和还原目标目录。"));
        return;
    }
     m_isRestore = true;
     setBusy(true);
    m_processedFiles.clear();
    m_currentSource = QString::fromStdString(req.pkg);
    m_currentDest   = QString::fromStdString(req.destDir);
     // 底部进度栏将在任务开始时自动显示进度
    if (!m_backend->startRestore(req)) {
    // backend busy
        setBusy(false);
    }
}

void MainWindow::onCancel() {
     m_backend->cancel();
}

void MainWindow::onProgress(const Progress& p) {
    const int pct = p.percent();

    // 更新内嵌进度栏
    m_progressInline->setValue(pct);
    m_progressStatus->setText(QStringLiteral("%1  |  %2/%3")
        .arg(QString::fromStdString(p.stage))
        .arg(p.doneFiles).arg(p.totalFiles));

    // 更新进度对话框
    if (!p.currentFile.empty()) {
        QString file = QString::fromUtf8(p.currentFile.data(),
            static_cast<int>(p.currentFile.size()));
        if (m_processedFiles.isEmpty() || m_processedFiles.last() != file)
            m_processedFiles.append(file);
    }
}

void MainWindow::onLog(int level, const QString& text) {
    Q_UNUSED(level);
    Q_UNUSED(text);
    // Backend log messages are not forwarded
}

void MainWindow::onFinished(bool success, const QString& summary) {
     setBusy(false);
     m_progressInline->setValue(success ? 100 : m_progressInline->value());
     m_progressStatus->setText(success ? QStringLiteral("完成") : QStringLiteral("结束"));
 
     // 记录到会话历史
     {
         HistoryEntry entry;
         entry.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
         entry.operation = m_isRestore ? QStringLiteral("还原") : QStringLiteral("备份");
         entry.fileCount = m_processedFiles.size();
         entry.files     = m_processedFiles;
         entry.source    = m_currentSource;
         entry.dest      = m_currentDest;
         entry.status    = success ? QStringLiteral("成功") : QStringLiteral("结束");
         entry.summary   = summary;
         m_sessionHistory.addEntry(entry);
     }
 
     showCompletionSummary(success, summary);
}

void MainWindow::onFailed(const QString& reason) {
     setBusy(false);
     m_progressStatus->setText(QStringLiteral("失败"));
 
     // 记录到会话历史
     {
         HistoryEntry entry;
         entry.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
         entry.operation = m_isRestore ? QStringLiteral("还原") : QStringLiteral("备份");
         entry.fileCount = m_processedFiles.size();
         entry.files     = m_processedFiles;
         entry.source    = m_currentSource;
         entry.dest      = m_currentDest;
         entry.status    = QStringLiteral("失败");
         entry.summary   = reason;
         m_sessionHistory.addEntry(entry);
     }
 
     QMessageBox::critical(this, QStringLiteral("任务失败"), reason);
}

 void MainWindow::showCompletionSummary(bool success, const QString& summary) {
     // 把已处理的文件路径保存在历史条目中（已完成）
     // QMessageBox 简要提示
     QString msg;
     if (success) {
         msg = QStringLiteral("任务已完成！\n\n共处理 %1 个文件。\n点击右上角「历史任务」查看详情。")
             .arg(m_processedFiles.size());
     } else {
         msg = QStringLiteral("任务失败\n\n") + summary;
     }
     QMessageBox::information(this, QStringLiteral("任务结果"), msg);
 }

 void MainWindow::setBusy(bool busy) {
     m_backup->setBusy(busy);
     m_restore->setBusy(busy);
     m_filter->setBusy(busy);
     // 运行中锁定标签切换，避免误操作
     m_tabs->tabBar()->setEnabled(!busy);
 }
 
 void MainWindow::closeEvent(QCloseEvent* event) {
     // SessionHistory 析构时自动清理临时文件
     event->accept();
 }
 
 } // namespace pbackup::ui
 
