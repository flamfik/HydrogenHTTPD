#pragma once
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
class Logger {
public:
    Logger(const std::filesystem::path& accessPath, const std::filesystem::path& errorPath);
    void access(const std::string& ip, const std::string& scheme, const std::string& method, const std::string& target, int status, std::size_t bytes);
    void error(const std::string& message);
private:
    std::string now() const;
    std::mutex mutex_;
    std::ofstream access_;
    std::ofstream error_;
};
