#include "ThreadPool.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
int main() {
    ThreadPool pool(2, 4);
    std::atomic<int> counter{0};
    assert(pool.enqueue([&]{ ++counter; }));
    assert(pool.enqueue([&]{ ++counter; }));
    assert(pool.enqueue([&]{ ++counter; }));
    for (int i = 0; i < 50 && counter.load() < 3; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(counter.load() == 3);
    pool.shutdown();
    std::cout << "thread pool tests passed\n";
    return 0;
}
