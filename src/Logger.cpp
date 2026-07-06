#include "Logger.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
Logger::Logger(const std::filesystem::path& accessPath, const std::filesystem::path& errorPath) {
    std::filesystem::create_directories(accessPath.parent_path());
    std::filesystem::create_directories(errorPath.parent_path());
    access_.open(accessPath, std::ios::app);
    error_.open(errorPath, std::ios::app);
    if (!access_) std::cerr << "Warning: cannot open access log: " << accessPath << "\n";
    if (!error_) std::cerr << "Warning: cannot open error log: " << errorPath << "\n";
}
void Logger::access(const std::string& ip, const std::string& scheme, const std::string& method, const std::string& target, int status, std::size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (access_) access_ << now() << " ip=" << ip << " scheme=" << scheme << " method=" << method << " target=\"" << target << "\" status=" << status << " bytes=" << bytes << "\n";
}
void Logger::error(const std::string& message) { std::lock_guard<std::mutex> lock(mutex_); if (error_) error_ << now() << " " << message << "\n"; }
std::string Logger::now() const {
    auto current = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(current);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
