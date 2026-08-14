#include "StaticFileCache.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;

int main() {
    const auto dir = fs::temp_directory_path() / "hydrogen_static_cache_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const auto file = dir / "index.txt";

    {
        std::ofstream out(file, std::ios::binary);
        out << "first";
    }

    StaticFileCache cache(true, 4, 16, 1024 * 1024, 1024, 0);
    auto first = cache.get(file);
    assert(first);
    assert(*first->body == "first");
    assert(cache.misses() == 1);

    auto second = cache.get(file);
    assert(second);
    assert(*second->body == "first");
    assert(cache.hits() >= 1);
    assert(cache.entries() == 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "second-value";
    }

    auto third = cache.get(file);
    assert(third);
    assert(*third->body == "second-value");
    assert(third->etag != first->etag);

    const auto big = dir / "big.bin";
    {
        std::ofstream out(big, std::ios::binary);
        out << std::string(2048, 'x');
    }
    auto bypass = cache.get(big);
    assert(bypass);
    assert(bypass->body->size() == 2048);
    assert(cache.entries() == 1);

    fs::remove_all(dir);
    std::cout << "static file cache tests passed\n";
    return 0;
}
