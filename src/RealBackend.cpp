#include "RealBackend.h"

#include "core/backup_task.h"
#include "core/encoding.h"
#include "core/filter.h"
#include "core/types.h"

#include <exception>

#include <thread>

namespace pbackup::ui {
namespace {

std::filesystem::path toFsPath(const std::string& value) {
    return std::filesystem::path(value);
}

core::FilterRules toCoreFilter(const FilterSpec& filter) {
    core::FilterRules rules;
    rules.includePath = filter.includePath;
    rules.excludePath = filter.excludePath;
    rules.nameGlob = filter.nameGlob;
    rules.typeFilter = core::parseTypeFilter(filter.typeFilter);
    rules.sizeMin = core::parseOptionalSize(filter.sizeMin);
    rules.sizeMax = core::parseOptionalSize(filter.sizeMax);
    rules.mtimeAfterNs = core::parseOptionalDateToNs(filter.mtimeAfter, false);
    rules.mtimeBeforeNs = core::parseOptionalDateToNs(filter.mtimeBefore, true);
    rules.ownerSid = filter.ownerSid;
    return rules;
}



Progress toUiProgress(const core::Progress& p) {
    Progress out;
    out.totalBytes = p.totalBytes;
    out.doneBytes = p.doneBytes;
    out.totalFiles = p.totalFiles;
    out.doneFiles = p.doneFiles;
    out.currentFile = p.currentFile;
    out.stage = std::string(core::stageName(p.stage));
    for (const auto& warning : p.warnings) {
        out.warnings.push_back(warning);
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

std::string errorSummary(const std::exception& ex) {
    return std::string(ex.what());
}

} // namespace

RealBackend::~RealBackend() {
    m_cancel = true;
    if (m_thread.joinable()) m_thread.join();
}

std::unique_ptr<BackendAdapter> createRealBackend() {
    return std::make_unique<RealBackend>();
}

bool RealBackend::startBackup(const BackupRequest& req, const FilterSpec& filter) {
    if (m_running.load()) return false;
    m_running = true;
    m_cancel = false;
    if (m_thread.joinable()) m_thread.join();
    m_thread = std::thread(&RealBackend::runBackup, this, req, filter);
    return true;
}

bool RealBackend::startRestore(const RestoreRequest& req) {
    if (m_running.load()) return false;
    m_running = true;
    m_cancel = false;
    if (m_thread.joinable()) m_thread.join();
    m_thread = std::thread(&RealBackend::runRestore, this, req);
    return true;
}

void RealBackend::runBackup(BackupRequest req, FilterSpec filter) {
    try {
        core::BackupOptions opt;
        opt.sourceRoot = toFsPath(req.sourceDir);
        opt.outputPkg = toFsPath(req.outputPkg);
        opt.compress = req.compress;
        opt.encrypt = req.encrypt;
        if (!req.password.empty()) opt.password = req.password;
        opt.filters = toCoreFilter(filter);

        core::RunContext context;
        context.cancel = &m_cancel;
        context.progress = [this](const core::Progress& p) {
            emitProgress(toUiProgress(p));
            return !m_cancel.load();
        };
        context.log = [this](core::LogLevel level, const std::string& text) {
            emitLog(toUiLogLevel(level), text);
        };

        emitLog(0, "RealBackend 已接管备份任务");
        core::BackupTask task(std::move(opt));
        task.run(context);
        m_running = false;
        emitFinished(true, "真实备份完成");
    } catch (const std::exception& ex) {
        const std::string reason = errorSummary(ex);
        m_running = false;
        emitLog(2, reason);
        emitFailed(reason);
    }
}

void RealBackend::runRestore(RestoreRequest req) {
    try {
        core::RestoreOptions opt;
        opt.pkg = toFsPath(req.pkg);
        opt.destRoot = toFsPath(req.destDir);
        opt.overwrite = req.overwrite;
        if (!req.password.empty()) opt.password = req.password;

        core::RunContext context;
        context.cancel = &m_cancel;
        context.progress = [this](const core::Progress& p) {
            emitProgress(toUiProgress(p));
            return !m_cancel.load();
        };
        context.log = [this](core::LogLevel level, const std::string& text) {
            emitLog(toUiLogLevel(level), text);
        };

        emitLog(0, "RealBackend 已接管还原任务");
        core::RestoreTask task(std::move(opt));
        task.run(context);
        m_running = false;
        emitFinished(true, "真实还原完成");
    } catch (const std::exception& ex) {
        const std::string reason = errorSummary(ex);
        m_running = false;
        emitLog(2, reason);
        emitFailed(reason);
    }
}

} // namespace pbackup::ui