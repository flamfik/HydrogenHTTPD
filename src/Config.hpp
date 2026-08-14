#pragma once
#include "Auth.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct ServerConfig {
    unsigned short port = 8080;
    bool enableTls = false;
    unsigned short tlsPort = 8443;
    unsigned short publicHttpsPort = 443;
    std::filesystem::path tlsCertFile = "certs/server.crt";
    std::filesystem::path tlsKeyFile = "certs/server.key";

    bool forceHttps = false;
    int httpsRedirectStatus = 308;
    bool exposeServerHeader = false;

    std::filesystem::path defaultRoot = "www";
    std::filesystem::path accessLog = "logs/access.log";
    std::filesystem::path errorLog = "logs/error.log";
    std::filesystem::path auditLog = "logs/audit.log";
    std::uintmax_t auditRotateBytes = 10 * 1024 * 1024;
    std::size_t auditRotateKeep = 5;
    std::filesystem::path authSecretsFile;
    bool authHotReload = true;
    unsigned int authReloadIntervalSeconds = 0;

    std::size_t maxRequestBytes = 16 * 1024;
    std::size_t maxBodyBytes = 1024 * 1024;
    std::size_t maxHeaderLineBytes = 4096;
    std::size_t maxHeaders = 100;
    std::size_t maxUriBytes = 2048;
    std::size_t maxMethodBytes = 16;
    bool enableRateLimiter = true;
    std::size_t rateLimitPerMinute = 120;
    std::size_t rateLimiterShards = 64;
    unsigned int readTimeoutSeconds = 5;

    std::size_t ioThreads = 2;
    std::size_t workerThreads = 0;
    std::size_t workerThreadMultiplier = 4;
    std::size_t maxPendingConnections = 4096;
    int listenBacklog = 4096;
    bool tcpNoDelay = true;
    bool tcpKeepAlive = true;
    int socketReceiveBufferBytes = 0;
    int socketSendBufferBytes = 0;

    bool asyncAccessLog = true;
    bool enableAccessLog = true;
    std::size_t accessLogSampleRate = 1;
    std::size_t accessLogQueueCapacity = 65536;
    unsigned int accessLogFlushIntervalMs = 100;

    bool enableStaticCache = true;
    std::size_t staticCacheShards = 32;
    std::size_t staticCacheMaxEntries = 4096;
    std::uintmax_t staticCacheMaxBytes = 256ULL * 1024ULL * 1024ULL;
    std::uintmax_t staticCacheMaxFileBytes = 4ULL * 1024ULL * 1024ULL;
    unsigned int staticCacheRevalidateMs = 1000;
    std::string staticCacheControl = "public, max-age=60";

    bool denyHiddenFiles = true;
    bool allowDotWellKnown = true;
    bool blockSensitiveFiles = true;

    bool enableHtaccess = false;
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

    bool enableUploads = false;
    std::string uploadEndpoint = "/__hydrogen/upload";
    std::filesystem::path uploadDirectory = "uploads";
    std::filesystem::path uploadSpoolDirectory = "tmp/uploads";
    std::size_t maxMultipartParts = 16;
    std::size_t maxUploadFileBytes = 1024 * 1024;
    std::size_t maxUploadFieldBytes = 16 * 1024;

    bool enableEndpointAuth = false;
    std::string authBearerToken;
    std::vector<AuthTokenRule> authTokens;

    bool enableAdminStatus = false;
    std::string adminStatusEndpoint = "/__hydrogen/admin/status";

    std::unordered_map<std::string, std::filesystem::path> virtualHosts;
};

class ConfigLoader {
public:
    static ServerConfig load(const std::filesystem::path& path);
private:
    static std::string trim(std::string value);
    static bool parseBool(const std::string& value);
};
