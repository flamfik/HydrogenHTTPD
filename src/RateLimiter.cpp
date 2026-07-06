#include "RateLimiter.hpp"
RateLimiter::RateLimiter(std::size_t maxPerMinute) : maxPerMinute_(maxPerMinute) {}
bool RateLimiter::allow(const std::string& key) {
    using namespace std::chrono;
    std::lock_guard<std::mutex> lock(mutex_);
    if (maxPerMinute_ == 0) return false;
    auto now = steady_clock::now();
    auto& bucket = buckets_[key];
    if (bucket.windowStart.time_since_epoch().count() == 0) { bucket.windowStart = now; bucket.count = 0; }
    if (duration_cast<seconds>(now - bucket.windowStart).count() >= 60) { bucket.windowStart = now; bucket.count = 0; }
    if (bucket.count >= maxPerMinute_) return false;
    ++bucket.count;
    return true;
}
