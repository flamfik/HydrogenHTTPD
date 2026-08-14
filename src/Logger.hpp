#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

class Logger {
public:
    Logger(
        const std::filesystem::path& accessPath,
        const std::filesystem::path& errorPath,
        const std::filesystem::path& auditPath,
        std::uintmax_t auditRotateBytes,
        std::size_t auditRotateKeep,
        bool asyncAccessLog = true,
        bool enableAccessLog = true,
        std::size_t accessLogSampleRate = 1,
        std::size_t accessQueueCapacity = 65536,
        unsigned int accessFlushIntervalMs = 100
    );
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void access(
        const std::string& ip,
        const std::string& scheme,
        const std::string& method,
        const std::string& target,
        int status,
        std::size_t bytes
    );

    void error(const std::string& message);
    void audit(const std::string& message);
    void flushAccess();

    std::uint64_t droppedAccessLogs() const noexcept {
        return droppedAccessLogs_.load(std::memory_order_relaxed);
    }

private:
    std::string now() const;
    void openAudit();
    void rotateAuditIfNeeded(std::size_t incomingBytes);
    void accessWriterLoop();
    void writeAccessBatch(std::deque<std::string>& batch);

    std::mutex fileMutex_;
    std::mutex accessFileMutex_;
    std::ofstream access_;
    std::ofstream error_;
    std::ofstream audit_;
    std::filesystem::path auditPath_;
    std::uintmax_t auditRotateBytes_ = 0;
    std::size_t auditRotateKeep_ = 0;

    bool asyncAccessLog_ = true;
    bool enableAccessLog_ = true;
    std::size_t accessLogSampleRate_ = 1;
    std::size_t accessQueueCapacity_ = 65536;
    std::chrono::milliseconds accessFlushInterval_{100};
    std::atomic<std::uint64_t> accessCounter_{0};
    std::atomic<std::uint64_t> droppedAccessLogs_{0};
    std::mutex accessQueueMutex_;
    std::condition_variable accessCv_;
    std::deque<std::string> accessQueue_;
    std::thread accessWriter_;
    bool stopping_ = false;
};
