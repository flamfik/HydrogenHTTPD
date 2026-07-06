#include "ThreadPool.hpp"
#include <stdexcept>
ThreadPool::ThreadPool(std::size_t workerCount, std::size_t maxQueueSize) : maxQueueSize_(maxQueueSize) {
    if (workerCount == 0) throw std::runtime_error("ThreadPool workerCount must be greater than 0");
    if (maxQueueSize == 0) throw std::runtime_error("ThreadPool maxQueueSize must be greater than 0");
    workers_.reserve(workerCount);
    for (std::size_t i = 0; i < workerCount; ++i) workers_.emplace_back(&ThreadPool::workerLoop, this);
}
ThreadPool::~ThreadPool() { shutdown(); }
bool ThreadPool::enqueue(std::function<void()> task) {
    { std::lock_guard<std::mutex> lock(mutex_); if (stopping_ || tasks_.size() >= maxQueueSize_) return false; tasks_.push(std::move(task)); }
    cv_.notify_one();
    return true;
}
void ThreadPool::shutdown() {
    { std::lock_guard<std::mutex> lock(mutex_); if (stopping_) return; stopping_ = true; }
    cv_.notify_all();
    for (auto& worker : workers_) if (worker.joinable()) worker.join();
}
std::size_t ThreadPool::queued() const { std::lock_guard<std::mutex> lock(mutex_); return tasks_.size(); }
void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        { std::unique_lock<std::mutex> lock(mutex_); cv_.wait(lock, [&] { return stopping_ || !tasks_.empty(); });
          if (stopping_ && tasks_.empty()) return; task = std::move(tasks_.front()); tasks_.pop(); }
        task();
    }
}
