#include "StaticFileCache.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;

StaticFileCache::StaticFileCache(
    bool enabled,
    std::size_t shardCount,
    std::size_t maxEntries,
    std::uintmax_t maxBytes,
    std::uintmax_t maxFileBytes,
    unsigned int revalidateMilliseconds
) : enabled_(enabled),
    maxEntries_(maxEntries),
    maxBytes_(maxBytes),
    maxFileBytes_(maxFileBytes),
    revalidateInterval_(revalidateMilliseconds) {
    if (shardCount == 0) shardCount = 1;
    shards_.reserve(shardCount);
    for (std::size_t i = 0; i < shardCount; ++i) shards_.push_back(std::make_unique<Shard>());
}

std::size_t StaticFileCache::shardIndex(const std::string& key) const {
    return std::hash<std::string>{}(key) % shards_.size();
}

std::string StaticFileCache::makeEtag(std::uintmax_t size, fs::file_time_type modified) {
    const auto ticks = modified.time_since_epoch().count();
    std::ostringstream out;
    out << '"' << std::hex << size << '-' << static_cast<std::uint64_t>(ticks) << '"';
    return out.str();
}

void StaticFileCache::evictIfNeeded(Shard& shard, std::size_t shardMaxEntries, std::uintmax_t shardMaxBytes) {
    while ((!shard.entries.empty()) &&
           (shard.entries.size() > shardMaxEntries || shard.bytes > shardMaxBytes)) {
        auto victim = shard.entries.end();
        std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
        for (auto it = shard.entries.begin(); it != shard.entries.end(); ++it) {
            if (it->second.lastAccess < oldest) {
                oldest = it->second.lastAccess;
                victim = it;
            }
        }
        if (victim == shard.entries.end()) break;
        const auto removed = victim->second.file ? victim->second.file->size : 0;
        shard.bytes -= removed;
        bytes_.fetch_sub(removed, std::memory_order_relaxed);
        entries_.fetch_sub(1, std::memory_order_relaxed);
        shard.entries.erase(victim);
        evictions_.fetch_add(1, std::memory_order_relaxed);
    }
}

std::shared_ptr<const CachedStaticFile> StaticFileCache::get(const fs::path& path) {
    const std::string key = path.string();
    const auto now = std::chrono::steady_clock::now();
    auto& shard = *shards_[shardIndex(key)];

    if (enabled_) {
        std::shared_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.entries.find(key);
        if (it != shard.entries.end() && now - it->second.validatedAt < revalidateInterval_) {
            hits_.fetch_add(1, std::memory_order_relaxed);
            return it->second.file;
        }
    }

    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) return {};
    auto modified = fs::last_write_time(path, ec);
    if (ec) modified = fs::file_time_type::min();

    if (enabled_) {
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.entries.find(key);
        if (it != shard.entries.end() && modified == it->second.modified && size == it->second.file->size) {
            it->second.validatedAt = now;
            hits_.fetch_add(1, std::memory_order_relaxed);
            return it->second.file;
        }
    }

    misses_.fetch_add(1, std::memory_order_relaxed);
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    auto body = std::make_shared<std::string>();
    body->resize(static_cast<std::size_t>(size));
    if (size > 0) file.read(body->data(), static_cast<std::streamsize>(size));
    if (!file && size > 0) return {};

    auto cached = std::make_shared<CachedStaticFile>(CachedStaticFile{body, makeEtag(size, modified), size});

    if (!enabled_ || maxEntries_ == 0 || maxBytes_ == 0 || size > maxFileBytes_ || size > maxBytes_) {
        return cached;
    }

    const std::size_t shardMaxEntries = std::max<std::size_t>(1, (maxEntries_ + shards_.size() - 1) / shards_.size());
    const std::uintmax_t shardMaxBytes = std::max<std::uintmax_t>(1, (maxBytes_ + shards_.size() - 1) / shards_.size());

    {
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto existing = shard.entries.find(key);
        if (existing != shard.entries.end()) {
            const auto oldSize = existing->second.file ? existing->second.file->size : 0;
            shard.bytes -= oldSize;
            bytes_.fetch_sub(oldSize, std::memory_order_relaxed);
        } else {
            entries_.fetch_add(1, std::memory_order_relaxed);
        }

        shard.entries[key] = Entry{cached, modified, now, clock_.fetch_add(1, std::memory_order_relaxed) + 1};
        shard.bytes += size;
        bytes_.fetch_add(size, std::memory_order_relaxed);
        evictIfNeeded(shard, shardMaxEntries, shardMaxBytes);
    }

    return cached;
}
