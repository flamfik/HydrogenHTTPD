#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>

namespace {
void createParentDirectory(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
}
}

Logger::Logger(
    const std::filesystem::path& accessPath,
    const std::filesystem::path& errorPath,
    const std::filesystem::path& auditPath,
    std::uintmax_t auditRotateBytes,
    std::size_t auditRotateKeep,
    bool asyncAccessLog,
    bool enableAccessLog,
    std::size_t accessLogSampleRate,
    std::size_t accessQueueCapacity,
    unsigned int accessFlushIntervalMs
) : auditPath_(auditPath),
    auditRotateBytes_(auditRotateBytes),
    auditRotateKeep_(auditRotateKeep),
    asyncAccessLog_(asyncAccessLog),
    enableAccessLog_(enableAccessLog),
    accessLogSampleRate_(accessLogSampleRate == 0 ? 1 : accessLogSampleRate),
    accessQueueCapacity_(accessQueueCapacity == 0 ? 1 : accessQueueCapacity),
    accessFlushInterval_(accessFlushIntervalMs == 0 ? 1 : accessFlushIntervalMs) {
    createParentDirectory(accessPath);
    createParentDirectory(errorPath);
    createParentDirectory(auditPath_);

    if (enableAccessLog_) access_.open(accessPath, std::ios::app);
    error_.open(errorPath, std::ios::app);
    openAudit();

    if (enableAccessLog_ && !access_) std::cerr << "Warning: cannot open access log: " << accessPath << "\n";
    if (!error_) std::cerr << "Warning: cannot open error log: " << errorPath << "\n";
    if (!audit_) std::cerr << "Warning: cannot open audit log: " << auditPath_ << "\n";

    if (enableAccessLog_ && asyncAccessLog_) accessWriter_ = std::thread(&Logger::accessWriterLoop, this);
}

Logger::~Logger() {
    if (enableAccessLog_ && asyncAccessLog_) {
        {
            std::lock_guard<std::mutex> lock(accessQueueMutex_);
            stopping_ = true;
        }
        accessCv_.notify_all();
        if (accessWriter_.joinable()) accessWriter_.join();
    }
    flushAccess();
}

void Logger::access(
    const std::string& ip,
    const std::string& scheme,
    const std::string& method,
    const std::string& target,
    int status,
    std::size_t bytes
) {
    if (!enableAccessLog_) return;

    const auto sequence = accessCounter_.fetch_add(1, std::memory_order_relaxed);
    if ((sequence % accessLogSampleRate_) != 0) return;

    std::ostringstream line;
    line << now()
         << " ip=" << ip
         << " scheme=" << scheme
         << " method=" << method
         << " target=\"" << target << "\""
         << " status=" << status
         << " bytes=" << bytes
         << "\n";

    if (!asyncAccessLog_) {
        std::lock_guard<std::mutex> lock(accessFileMutex_);
        if (access_) access_ << line.str();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(accessQueueMutex_);
        if (accessQueue_.size() >= accessQueueCapacity_) {
            droppedAccessLogs_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        accessQueue_.push_back(line.str());
    }
    accessCv_.notify_one();
}

void Logger::writeAccessBatch(std::deque<std::string>& batch) {
    if (batch.empty()) return;
    std::lock_guard<std::mutex> lock(accessFileMutex_);
    if (!access_) return;
    for (const auto& line : batch) access_ << line;
    access_.flush();
}

void Logger::accessWriterLoop() {
    std::deque<std::string> batch;
    while (true) {
        {
            std::unique_lock<std::mutex> lock(accessQueueMutex_);
            accessCv_.wait_for(lock, accessFlushInterval_, [&] { return stopping_ || !accessQueue_.empty(); });
            accessQueue_.swap(batch);
            if (stopping_ && batch.empty()) break;
        }

        writeAccessBatch(batch);
        batch.clear();
    }
}

void Logger::flushAccess() {
    if (!enableAccessLog_) return;

    if (asyncAccessLog_) {
        std::deque<std::string> batch;
        {
            std::lock_guard<std::mutex> lock(accessQueueMutex_);
            accessQueue_.swap(batch);
        }
        writeAccessBatch(batch);
    }

    std::lock_guard<std::mutex> lock(accessFileMutex_);
    if (access_) access_.flush();
}

void Logger::error(const std::string& message) {
    std::lock_guard<std::mutex> lock(fileMutex_);
    if (error_) {
        error_ << now() << " " << message << "\n";
        error_.flush();
    }
}

void Logger::openAudit() {
    audit_.open(auditPath_, std::ios::app);
}

void Logger::rotateAuditIfNeeded(std::size_t incomingBytes) {
    if (auditRotateBytes_ == 0 || auditRotateKeep_ == 0) return;

    std::error_code ec;
    const auto currentSize = std::filesystem::exists(auditPath_, ec)
        ? std::filesystem::file_size(auditPath_, ec)
        : 0;

    if (ec || currentSize + incomingBytes <= auditRotateBytes_) return;

    audit_.flush();
    audit_.close();

    const auto oldest = std::filesystem::path(auditPath_.string() + "." + std::to_string(auditRotateKeep_));
    std::filesystem::remove(oldest, ec);
    ec.clear();

    for (std::size_t index = auditRotateKeep_; index > 1; --index) {
        const auto from = std::filesystem::path(auditPath_.string() + "." + std::to_string(index - 1));
        const auto to = std::filesystem::path(auditPath_.string() + "." + std::to_string(index));
        if (std::filesystem::exists(from, ec)) {
            ec.clear();
            std::filesystem::remove(to, ec);
            ec.clear();
            std::filesystem::rename(from, to, ec);
        }
        ec.clear();
    }

    const auto first = std::filesystem::path(auditPath_.string() + ".1");
    if (std::filesystem::exists(auditPath_, ec)) {
        ec.clear();
        std::filesystem::remove(first, ec);
        ec.clear();
        std::filesystem::rename(auditPath_, first, ec);
    }

    openAudit();
}

void Logger::audit(const std::string& message) {
    const std::string line = now() + " " + message + "\n";

    std::lock_guard<std::mutex> lock(fileMutex_);
    rotateAuditIfNeeded(line.size());

    if (audit_) {
        audit_ << line;
        audit_.flush();
    }
}

std::string Logger::now() const {
    const auto current = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(current);
    std::tm tm{};

#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::ostringstream output;
    output << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return output.str();
}
