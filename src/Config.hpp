#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>

struct ServerConfig {
    unsigned short port = 8080;
    bool enableTls = false;
    unsigned short tlsPort = 8443;
    std::filesystem::path tlsCertFile = "certs/server.crt";
    std::filesystem::path tlsKeyFile = "certs/server.key";

    std::filesystem::path defaultRoot = "www";
    std::filesystem::path accessLog = "logs/access.log";
    std::filesystem::path errorLog = "logs/error.log";

    std::size_t maxRequestBytes = 16 * 1024;
    std::size_t rateLimitPerMinute = 120;
    unsigned int readTimeoutSeconds = 5;

    std::size_t workerThreads = 4;
    std::size_t maxPendingConnections = 256;

    bool enableHtaccess = true;
    std::string htaccessFilename = ".htaccess";
    std::size_t maxHtaccessDepth = 8;

    bool enablePhp = false;
    std::string phpExtension = ".php";
    std::string phpFastcgiHost = "127.0.0.1";
    unsigned short phpFastcgiPort = 9000;
    unsigned int phpFastcgiConnectTimeoutSeconds = 3;
    unsigned int phpFastcgiReadTimeoutSeconds = 10;

    bool enableSql = false;
    std::filesystem::path sqliteDatabase = "sql/hydrogen.db";

    std::unordered_map<std::string, std::filesystem::path> virtualHosts;
};

class ConfigLoader {
public:
    static ServerConfig load(const std::filesystem::path& path);
private:
    static std::string trim(std::string value);
    static bool parseBool(const std::string& value);
};
