#include "SqliteModule.hpp"

#if defined(HYDROGENHTTPD_ENABLE_SQLITE)
#include <sqlite3.h>
#endif

#include <filesystem>

SqliteModule::SqliteModule(std::filesystem::path databasePath)
    : databasePath_(std::move(databasePath)) {}

bool SqliteModule::initialize() {
#if defined(HYDROGENHTTPD_ENABLE_SQLITE)
    std::filesystem::create_directories(databasePath_.parent_path());

    sqlite3* db = nullptr;
    if (sqlite3_open(databasePath_.string().c_str(), &db) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS hydrogen_events ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        "event_type TEXT NOT NULL,"
        "message TEXT NOT NULL"
        ");";

    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
    sqlite3_close(db);
    return rc == SQLITE_OK;
#else
    return false;
#endif
}

SqlQueryResult SqliteModule::executeSafeStatement(const std::string& sql) {
#if defined(HYDROGENHTTPD_ENABLE_SQLITE)
    SqlQueryResult result;
    sqlite3* db = nullptr;
    if (sqlite3_open(databasePath_.string().c_str(), &db) != SQLITE_OK) {
        result.error = "failed to open database";
        if (db) sqlite3_close(db);
        return result;
    }

    char* err = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        result.error = err ? err : "sqlite error";
        if (err) sqlite3_free(err);
        sqlite3_close(db);
        return result;
    }

    sqlite3_close(db);
    result.ok = true;
    return result;
#else
    return {false, "SQLite module disabled at build time"};
#endif
}

SqlQueryResult SqliteModule::recordEvent(const std::string& eventType, const std::string& message) {
#if defined(HYDROGENHTTPD_ENABLE_SQLITE)
    SqlQueryResult result;
    sqlite3* db = nullptr;
    if (sqlite3_open(databasePath_.string().c_str(), &db) != SQLITE_OK) {
        result.error = "failed to open database";
        if (db) sqlite3_close(db);
        return result;
    }

    const char* sql = "INSERT INTO hydrogen_events(event_type, message) VALUES(?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.error = "prepare failed";
        sqlite3_close(db);
        return result;
    }

    sqlite3_bind_text(stmt, 1, eventType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, message.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (rc != SQLITE_DONE) {
        result.error = "insert failed";
        return result;
    }

    result.ok = true;
    return result;
#else
    return {false, "SQLite module disabled at build time"};
#endif
}
