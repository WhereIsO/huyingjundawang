#include "MainWindow.h"
#include "BackupTab.h"
#include "RestoreTab.h"
#include "FilterTab.h"
#include "LogPanel.h"
#include "Theme.h"

#include <QTabWidget>
#include <QTabBar>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QSplitter>
#include <QGroupBox>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>

namespace pbackup::ui {
namespace {

bool realBackendRequested() {
    return qgetenv("BACKUP_BACKEND_MODE").toLower() == "real";
}

QString backendModeText() {
    return realBackendRequested() ? QStringLiteral("真实后端") : QStringLiteral("演示后端");
}

class HeaderPanel : public QWidget {
public:
    explicit HeaderPanel(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(86);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(22, 14, 22, 14);
        layout->setSpacing(14);

        auto* textCol = new QVBoxLayout();
        textCol->setSpacing(4);

        auto* title = new QLabel(QStringLiteral("数据备份工具"), this);
        title->setFont(Theme::titleFont());
        title->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::textColor()));

        auto* subtitle = new QLabel(QStringLiteral("备份  筛选  还原"), this);
        subtitle->setFont(Theme::appFont());
        subtitle->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::mutedTextColor()));

        textCol->addWidget(title);
        textCol->addWidget(subtitle);
        layout->addLayout(textCol, 1);

        auto* badge = new QLabel(backendModeText(), this);
        badge->setFont(Theme::appFont());
        badge->setAlignment(Qt::AlignCenter);
        badge->setMinimumWidth(96);
        badge->setStyleSheet(QStringLiteral(
            "QLabel{background:%1;color:#FFFFFF;padding:7px 14px;border:0;}")
            .arg(realBackendRequested() ? Theme::successColor() : Theme::accentColor()));
        layout->addWidget(badge, 0, Qt::AlignRight | Qt::AlignVCenter);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(Theme::surfaceAltColor()));

        painter.fillRect(QRect(0, 0, 8, height()), QColor(Theme::primaryColor()));
        painter.setPen(QPen(QColor("#DDE7EA"), 1));
        for (int x = width() - 260; x < width() + 80; x += 18) {
            painter.drawLine(QPoint(x, 0), QPoint(x - 70, height()));
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#EAF1F3"));
        for (int y = 14; y < height(); y += 18) {
            painter.drawRect(QRect(width() - 180, y, 8, 8));
            painter.drawRect(QRect(width() - 132, y + 8, 8, 8));
        }
    }
};

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("数据备份工具"));
    // 初始尺寸收紧，保证在 1080p（含缩放）的可用工作区内完整可见；
    // 筛选页已内置滚动区，窗口再小也能滚动查看，不会被裁切。
    resize(1080, 680);
    setMinimumSize(860, 560);

    // 让 Progress 结构体能跨线程随信号传递
    qRegisterMetaType<Progress>("Progress");

    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("AppCentral"));
    central->setStyleSheet(Theme::styleSheet());
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(Theme::sectionSpacing());

    // ---- 后端 ----
    m_backend = createBackend(this);

    auto* header = new HeaderPanel(central);
    root->addWidget(header);

    // ---- 顶部标签页 ----
    m_tabs = new QTabWidget(central);
    m_tabs->setFont(Theme::appFont());
    m_backup  = new BackupTab(m_tabs);
    m_restore = new RestoreTab(m_tabs);
    m_filter  = new FilterTab(m_tabs);
    m_tabs->addTab(m_backup,  QStringLiteral("① 备份"));
    m_tabs->addTab(m_filter,  QStringLiteral("② 筛选"));
    m_tabs->addTab(m_restore, QStringLiteral("③ 还原"));

    // 截图/演示用：环境变量 BACKUP_UI_TAB=0/1/2 指定初始标签页
    {
        bool okTab = false;
        const int tabIdx = qgetenv("BACKUP_UI_TAB").toInt(&okTab);
        if (okTab && tabIdx >= 0 && tabIdx < m_tabs->count())
            m_tabs->setCurrentIndex(tabIdx);
    }

    // ---- 进度区 ----
    auto* progRow = new QVBoxLayout();
    m_status = new QLabel(QStringLiteral("就绪"), central);
    m_status->setFont(Theme::appFont());
    m_progress = new QProgressBar(central);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setMinimumHeight(28);
    m_progress->setFont(Theme::appFont());
    progRow->addWidget(m_status);
    progRow->addWidget(m_progress);

    // ---- 日志面板 ----
    auto* logBox = new QGroupBox(QStringLiteral("运行日志"), central);
    logBox->setFont(Theme::appFont());
    auto* logLay = new QVBoxLayout(logBox);
    m_log = new LogPanel(logBox);
    logLay->addWidget(m_log);

    // 上（标签页）下（进度+日志）用 Splitter，日志可拉伸
    auto* splitter = new QSplitter(Qt::Vertical, central);
    splitter->addWidget(m_tabs);
    auto* bottom = new QWidget(central);
    auto* bottomLay = new QVBoxLayout(bottom);
    bottomLay->setContentsMargins(0, 0, 0, 0);
    bottomLay->addLayout(progRow);
    bottomLay->addWidget(logBox);
    splitter->addWidget(bottom);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    // 截图/演示用：环境变量 BACKUP_UI_CAPTURE=1 时隐藏底部进度+日志面板，
    // 让标签页占满窗口，便于为文档单独抓取完整的筛选页（不影响正常使用）。
    if (qgetenv("BACKUP_UI_CAPTURE").toInt() == 1)
        bottom->hide();

    root->addWidget(splitter);
    setCentralWidget(central);

    wireBackend();

    connect(m_backup,  &BackupTab::startRequested,  this, &MainWindow::onBackupStart);
    connect(m_backup,  &BackupTab::cancelRequested, this, &MainWindow::onCancel);
    connect(m_restore, &RestoreTab::startRequested, this, &MainWindow::onRestoreStart);
    connect(m_restore, &RestoreTab::cancelRequested,this, &MainWindow::onCancel);

    if (realBackendRequested()) {
        m_log->appendSuccess(QStringLiteral("界面已就绪。当前为真实后端，将执行实际备份与还原。"));
    } else {
        m_log->appendInfo(QStringLiteral("界面已就绪。当前为演示后端（Mock），不会改动真实文件。"));
    }
}

void MainWindow::wireBackend() {
    connect(m_backend.get(), &BackendAdapter::progress, this, &MainWindow::onProgress);
    connect(m_backend.get(), &BackendAdapter::log,      this, &MainWindow::onLog);
    connect(m_backend.get(), &BackendAdapter::finished, this, &MainWindow::onFinished);
    connect(m_backend.get(), &BackendAdapter::failed,   this, &MainWindow::onFailed);
}

void MainWindow::onBackupStart() {
    const BackupRequest req = m_backup->buildRequest();
    if (req.sourceDir.isEmpty() || req.outputPkg.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先选择源目录和备份包保存路径。"));
        return;
    }
    if (req.encrypt && req.password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("已勾选加密，请填写密码。"));
        return;
    }
    const FilterSpec filter = m_filter->buildSpec();
    setBusy(true);
    m_progress->setValue(0);
    if (!m_backend->startBackup(req, filter)) {
        m_log->appendError(QStringLiteral("后端忙，无法开始新任务。"));
        setBusy(false);
    }
}

void MainWindow::onRestoreStart() {
    const RestoreRequest req = m_restore->buildRequest();
    if (req.pkg.isEmpty() || req.destDir.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先选择备份包和还原目标目录。"));
        return;
    }
    setBusy(true);
    m_progress->setValue(0);
    if (!m_backend->startRestore(req)) {
        m_log->appendError(QStringLiteral("后端忙，无法开始新任务。"));
        setBusy(false);
    }
}

void MainWindow::onCancel() {
    m_log->appendWarn(QStringLiteral("正在取消当前任务…"));
    m_backend->cancel();
}

void MainWindow::onProgress(const Progress& p) {
    m_progress->setValue(p.percent());
    m_status->setText(QStringLiteral("%1  |  %2/%3 文件  |  当前：%4")
        .arg(p.stage)
        .arg(p.doneFiles).arg(p.totalFiles)
        .arg(p.currentFile));
}

void MainWindow::onLog(int level, const QString& text) {
    m_log->appendLog(static_cast<LogPanel::Level>(level), text);
}

void MainWindow::onFinished(bool success, const QString& summary) {
    setBusy(false);
    m_progress->setValue(success ? 100 : m_progress->value());
    m_status->setText(success ? QStringLiteral("完成") : QStringLiteral("结束"));
    if (success) m_log->appendSuccess(summary);
    else         m_log->appendWarn(summary);
}

void MainWindow::onFailed(const QString& reason) {
    setBusy(false);
    m_status->setText(QStringLiteral("失败"));
    m_log->appendError(reason);
    QMessageBox::critical(this, QStringLiteral("任务失败"), reason);
}

void MainWindow::setBusy(bool busy) {
    m_backup->setBusy(busy);
    m_restore->setBusy(busy);
    m_filter->setBusy(busy);
    // 运行中锁定标签切换，避免误操作
    m_tabs->tabBar()->setEnabled(!busy);
}

} // namespace pbackup::ui
