#include "Logger.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main() {
    const auto dir = fs::temp_directory_path() / "hydrogen_logger_rotation_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const auto access = dir / "access.log";
    const auto error = dir / "error.log";
    const auto audit = dir / "audit.log";

    Logger logger(access, error, audit, 240, 2, true, true, 1, 128, 5);

    for (int i = 0; i < 50; ++i) {
        logger.access("127.0.0.1", "http", "GET", "/", 200, 123);
    }
    logger.flushAccess();
    assert(fs::exists(access));
    assert(fs::file_size(access) > 0);

    for (int i = 0; i < 30; ++i) {
        logger.audit(
            "event=test_rotation sequence=" + std::to_string(i) +
            " payload=\"abcdefghijklmnopqrstuvwxyz0123456789\""
        );
    }

    assert(fs::exists(audit));
    assert(fs::exists(fs::path(audit.string() + ".1")));
    assert(fs::exists(fs::path(audit.string() + ".2")));
    assert(!fs::exists(fs::path(audit.string() + ".3")));

    fs::remove_all(dir);
    std::cout << "logger rotation tests passed\n";
    return 0;
}
