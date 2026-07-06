#include "SqliteModule.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    auto db = fs::temp_directory_path() / "hydrogen_sqlite_test.db";
    fs::remove(db);

    SqliteModule sqlite(db);
    assert(sqlite.initialize());

    auto inserted = sqlite.recordEvent("test", "hello");
    assert(inserted.ok);

    fs::remove(db);
    std::cout << "sqlite tests passed\n";
    return 0;
}
