#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct CachedStaticFile {
    std::shared_ptr<const std::string> body;
    std::string etag;
    std::uintmax_t size = 0;
};

class StaticFileCache {
public:
    StaticFileCache(
        bool enabled,
        std::size_t shardCount,
        std::size_t maxEntries,
        std::uintmax_t maxBytes,
        std::uintmax_t maxFileBytes,
        unsigned int revalidateMilliseconds
    );

    std::shared_ptr<const CachedStaticFile> get(const std::filesystem::path& path);

    std::uint64_t hits() const noexcept { return hits_.load(std::memory_order_relaxed); }
    std::uint64_t misses() const noexcept { return misses_.load(std::memory_order_relaxed); }
    std::uint64_t evictions() const noexcept { return evictions_.load(std::memory_order_relaxed); }
    std::uintmax_t bytes() const noexcept { return bytes_.load(std::memory_order_relaxed); }
    std::size_t entries() const noexcept { return entries_.load(std::memory_order_relaxed); }

private:
    struct Entry {
        std::shared_ptr<const CachedStaticFile> file;
        std::filesystem::file_time_type modified;
        std::chrono::steady_clock::time_point validatedAt;
        std::uint64_t lastAccess = 0;
    };

    struct Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<std::string, Entry> entries;
        std::uintmax_t bytes = 0;
    };

    std::size_t shardIndex(const std::string& key) const;
    void evictIfNeeded(Shard& shard, std::size_t shardMaxEntries, std::uintmax_t shardMaxBytes);
    static std::string makeEtag(std::uintmax_t size, std::filesystem::file_time_type modified);

    bool enabled_;
    std::size_t maxEntries_;
    std::uintmax_t maxBytes_;
    std::uintmax_t maxFileBytes_;
    std::chrono::milliseconds revalidateInterval_;
    std::vector<std::unique_ptr<Shard>> shards_;
    std::atomic<std::uint64_t> clock_{0};
    std::atomic<std::uint64_t> hits_{0};
    std::atomic<std::uint64_t> misses_{0};
    std::atomic<std::uint64_t> evictions_{0};
    std::atomic<std::uintmax_t> bytes_{0};
    std::atomic<std::size_t> entries_{0};
};
