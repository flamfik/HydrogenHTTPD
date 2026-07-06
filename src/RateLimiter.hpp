#pragma once
#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
class RateLimiter {
public:
    explicit RateLimiter(std::size_t maxPerMinute);
    bool allow(const std::string& key);
private:
    struct Bucket { std::size_t count = 0; std::chrono::steady_clock::time_point windowStart; };
    std::size_t maxPerMinute_;
    std::mutex mutex_;
    std::unordered_map<std::string, Bucket> buckets_;
};
