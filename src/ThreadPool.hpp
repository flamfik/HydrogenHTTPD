#pragma once
#include <condition_variable>
#include <cstddef>
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
private:
    void workerLoop();
    std::size_t maxQueueSize_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
};
