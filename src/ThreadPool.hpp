#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    ThreadPool(std::size_t workerCount, std::size_t maxQueueSize);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    bool enqueue(std::function<void()> task);
    void shutdown();

    std::size_t queued() const;
    std::size_t active() const noexcept { return active_.load(std::memory_order_relaxed); }
    std::size_t workerCount() const noexcept { return workers_.size(); }
    std::uint64_t completed() const noexcept { return completed_.load(std::memory_order_relaxed); }
    std::uint64_t rejected() const noexcept { return rejected_.load(std::memory_order_relaxed); }

private:
    void workerLoop();

    std::size_t maxQueueSize_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
    std::atomic<std::size_t> active_{0};
    std::atomic<std::uint64_t> completed_{0};
    std::atomic<std::uint64_t> rejected_{0};
};
