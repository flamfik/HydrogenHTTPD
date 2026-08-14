#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class RateLimiter {
public:
    explicit RateLimiter(std::size_t maxPerMinute, std::size_t shardCount = 64);
    bool allow(const std::string& key);
    std::size_t bucketCount() const;

private:
    struct Bucket {
        std::size_t count = 0;
        std::chrono::steady_clock::time_point windowStart;
        std::chrono::steady_clock::time_point lastSeen;
    };

    struct Shard {
        mutable std::mutex mutex;
        std::unordered_map<std::string, Bucket> buckets;
        std::size_t operations = 0;
    };

    void cleanupShard(Shard& shard, std::chrono::steady_clock::time_point now);

    std::size_t maxPerMinute_;
    std::vector<std::unique_ptr<Shard>> shards_;
};
