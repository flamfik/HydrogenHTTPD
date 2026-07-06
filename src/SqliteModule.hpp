#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct SqlQueryResult {
    bool ok = false;
    std::string error;
};

class SqliteModule {
public:
    explicit SqliteModule(std::filesystem::path databasePath);

    bool initialize();
    SqlQueryResult executeSafeStatement(const std::string& sql);

    // Example app-safe operation: structured insert, not user-provided SQL.
    SqlQueryResult recordEvent(const std::string& eventType, const std::string& message);

private:
    std::filesystem::path databasePath_;
};
