#include "RealBackend.h"

#include "core/backup_task.h"
#include "core/encoding.h"
#include "core/filter.h"
#include "core/types.h"

#include <QDate>
#include <QtConcurrent>

#include <exception>

namespace pbackup::ui {
namespace {

std::string qStringToUtf8(const QString& value) {
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::filesystem::path qPathToFs(const QString& value) {
    return std::filesystem::path(value.toStdWString());
}

std::vector<std::string> qStringListToUtf8(const QStringList& values) {
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(values.size()));
    for (const QString& value : values) {
        if (!value.trimmed().isEmpty()) result.push_back(qStringToUtf8(value.trimmed()));
    }
    return result;
}

core::FilterRules toCoreFilter(const FilterSpec& filter) {
    core::FilterRules rules;
    rules.includePath = qStringListToUtf8(filter.includePath);
    rules.excludePath = qStringListToUtf8(filter.excludePath);
    rules.nameGlob = qStringToUtf8(filter.nameGlob.trimmed());
    rules.typeFilter = core::parseTypeFilter(qStringToUtf8(filter.typeFilter.trimmed()));
    rules.sizeMin = core::parseOptionalSize(qStringToUtf8(filter.sizeMin.trimmed()));
    rules.sizeMax = core::parseOptionalSize(qStringToUtf8(filter.sizeMax.trimmed()));
    rules.mtimeAfterNs = core::parseOptionalDateToNs(qStringToUtf8(filter.mtimeAfter.trimmed()), false);
    rules.mtimeBeforeNs = core::parseOptionalDateToNs(qStringToUtf8(filter.mtimeBefore.trimmed()), true);
    rules.ownerSid = qStringToUtf8(filter.ownerSid.trimmed());
    return rules;
}

QString fromUtf8(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

Progress toUiProgress(const core::Progress& p) {
    Progress out;
    out.totalBytes = static_cast<quint64>(p.totalBytes);
    out.doneBytes = static_cast<quint64>(p.doneBytes);
    out.totalFiles = static_cast<quint32>(p.totalFiles);
    out.doneFiles = static_cast<quint32>(p.doneFiles);
    out.currentFile = fromUtf8(p.currentFile);
    out.stage = QString::fromLatin1(core::stageName(p.stage));
    for (const auto& warning : p.warnings) {
        out.warnings << fromUtf8(warning);
    }
    return out;
}

int toUiLogLevel(core::LogLevel level) {
    switch (level) {
    case core::LogLevel::Info: return 0;
    case core::LogLevel::Warn: return 1;
    case core::LogLevel::Error: return 2;
    case core::LogLevel::Ok: return 3;
    }
    return 0;
}

QString errorSummary(const std::exception& ex) {
    return QString::fromUtf8(ex.what());
}

} // namespace

RealBackend::RealBackend(QObject* parent) : BackendAdapter(parent) {}

std::unique_ptr<BackendAdapter> createRealBackend(QObject* parent) {
    return std::make_unique<RealBackend>(parent);
}

bool RealBackend::startBackup(const BackupRequest& req, const FilterSpec& filter) {
    if (m_running.load()) return false;
    m_running = true;
    m_cancel = false;
    QtConcurrent::run([this, req, filter]() { runBackup(req, filter); });
    return true;
}

bool RealBackend::startRestore(const RestoreRequest& req) {
    if (m_running.load()) return false;
    m_running = true;
    m_cancel = false;
    QtConcurrent::run([this, req]() { runRestore(req); });
    return true;
}

void RealBackend::cancel() {
    m_cancel = true;
}

void RealBackend::runBackup(BackupRequest req, FilterSpec filter) {
    try {
        core::BackupOptions opt;
        opt.sourceRoot = qPathToFs(req.sourceDir);
        opt.outputPkg = qPathToFs(req.outputPkg);
        opt.compress = req.compress;
        opt.encrypt = req.encrypt;
        if (!req.password.isEmpty()) opt.password = qStringToUtf8(req.password);
        opt.filters = toCoreFilter(filter);

        core::RunContext context;
        context.cancel = &m_cancel;
        context.progress = [this](const core::Progress& p) {
            emit progress(toUiProgress(p));
            return !m_cancel.load();
        };
        context.log = [this](core::LogLevel level, const std::string& text) {
            emit log(toUiLogLevel(level), fromUtf8(text));
        };

        emit log(0, QStringLiteral("RealBackend 已接管备份任务"));
        core::BackupTask task(std::move(opt));
        task.run(context);
        m_running = false;
        emit finished(true, QStringLiteral("真实备份完成"));
    } catch (const std::exception& ex) {
        const QString reason = errorSummary(ex);
        m_running = false;
        emit log(2, reason);
        emit failed(reason);
    }
}

void RealBackend::runRestore(RestoreRequest req) {
    try {
        core::RestoreOptions opt;
        opt.pkg = qPathToFs(req.pkg);
        opt.destRoot = qPathToFs(req.destDir);
        opt.overwrite = req.overwrite;
        if (!req.password.isEmpty()) opt.password = qStringToUtf8(req.password);

        core::RunContext context;
        context.cancel = &m_cancel;
        context.progress = [this](const core::Progress& p) {
            emit progress(toUiProgress(p));
            return !m_cancel.load();
        };
        context.log = [this](core::LogLevel level, const std::string& text) {
            emit log(toUiLogLevel(level), fromUtf8(text));
        };

        emit log(0, QStringLiteral("RealBackend 已接管还原任务"));
        core::RestoreTask task(std::move(opt));
        task.run(context);
        m_running = false;
        emit finished(true, QStringLiteral("真实还原完成"));
    } catch (const std::exception& ex) {
        const QString reason = errorSummary(ex);
        m_running = false;
        emit log(2, reason);
        emit failed(reason);
    }
}

} // namespace pbackup::ui
