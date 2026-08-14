#include "RateLimiter.hpp"

RateLimiter::RateLimiter(std::size_t maxPerMinute, std::size_t shardCount)
    : maxPerMinute_(maxPerMinute) {
    if (shardCount == 0) shardCount = 1;
    shards_.reserve(shardCount);
    for (std::size_t i = 0; i < shardCount; ++i) shards_.push_back(std::make_unique<Shard>());
}

void RateLimiter::cleanupShard(Shard& shard, std::chrono::steady_clock::time_point now) {
    using namespace std::chrono;
    for (auto it = shard.buckets.begin(); it != shard.buckets.end();) {
        if (duration_cast<minutes>(now - it->second.lastSeen).count() >= 5) it = shard.buckets.erase(it);
        else ++it;
    }
}

bool RateLimiter::allow(const std::string& key) {
    using namespace std::chrono;
    if (maxPerMinute_ == 0) return false;

    auto& shard = *shards_[std::hash<std::string>{}(key) % shards_.size()];
    const auto now = steady_clock::now();
    std::lock_guard<std::mutex> lock(shard.mutex);

    if (++shard.operations % 4096 == 0) cleanupShard(shard, now);

    auto& bucket = shard.buckets[key];
    if (bucket.windowStart.time_since_epoch().count() == 0) {
        bucket.windowStart = now;
        bucket.count = 0;
    }
    bucket.lastSeen = now;

    if (duration_cast<seconds>(now - bucket.windowStart).count() >= 60) {
        bucket.windowStart = now;
        bucket.count = 0;
    }

    if (bucket.count >= maxPerMinute_) return false;
    ++bucket.count;
    return true;
}

std::size_t RateLimiter::bucketCount() const {
    std::size_t total = 0;
    for (const auto& shardPtr : shards_) {
        std::lock_guard<std::mutex> lock(shardPtr->mutex);
        total += shardPtr->buckets.size();
    }
    return total;
}
