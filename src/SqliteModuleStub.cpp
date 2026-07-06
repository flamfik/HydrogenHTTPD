#include "SqliteModule.hpp"

SqliteModule::SqliteModule(std::filesystem::path databasePath)
    : databasePath_(std::move(databasePath)) {}

bool SqliteModule::initialize() {
    return false;
}

SqlQueryResult SqliteModule::executeSafeStatement(const std::string&) {
    return {false, "SQLite module disabled at build time"};
}

SqlQueryResult SqliteModule::recordEvent(const std::string&, const std::string&) {
    return {false, "SQLite module disabled at build time"};
}
