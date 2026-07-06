#include "Config.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

ServerConfig ConfigLoader::load(const std::filesystem::path& path) {
    ServerConfig config;
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open config file: " + path.string());

    std::string line;
    while (std::getline(file, line)) {
        auto comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (key == "port") config.port = static_cast<unsigned short>(std::stoi(value));
        else if (key == "enable_tls") config.enableTls = parseBool(value);
        else if (key == "tls_port") config.tlsPort = static_cast<unsigned short>(std::stoi(value));
        else if (key == "tls_cert_file") config.tlsCertFile = value;
        else if (key == "tls_key_file") config.tlsKeyFile = value;
        else if (key == "default_root") config.defaultRoot = value;
        else if (key == "access_log") config.accessLog = value;
        else if (key == "error_log") config.errorLog = value;
        else if (key == "max_request_bytes") config.maxRequestBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "rate_limit_per_minute") config.rateLimitPerMinute = static_cast<std::size_t>(std::stoull(value));
        else if (key == "read_timeout_seconds") config.readTimeoutSeconds = static_cast<unsigned int>(std::stoul(value));
        else if (key == "worker_threads") config.workerThreads = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_pending_connections") config.maxPendingConnections = static_cast<std::size_t>(std::stoull(value));
        else if (key == "enable_htaccess") config.enableHtaccess = parseBool(value);
        else if (key == "htaccess_filename") config.htaccessFilename = value;
        else if (key == "max_htaccess_depth") config.maxHtaccessDepth = static_cast<std::size_t>(std::stoull(value));

        else if (key == "enable_php") config.enablePhp = parseBool(value);
        else if (key == "php_extension") config.phpExtension = value;
        else if (key == "php_fastcgi_host") config.phpFastcgiHost = value;
        else if (key == "php_fastcgi_port") config.phpFastcgiPort = static_cast<unsigned short>(std::stoi(value));
        else if (key == "php_fastcgi_connect_timeout_seconds") config.phpFastcgiConnectTimeoutSeconds = static_cast<unsigned int>(std::stoul(value));
        else if (key == "php_fastcgi_read_timeout_seconds") config.phpFastcgiReadTimeoutSeconds = static_cast<unsigned int>(std::stoul(value));

        else if (key == "enable_sql") config.enableSql = parseBool(value);
        else if (key == "sqlite_database") config.sqliteDatabase = value;

        else if (key.rfind("vhost.", 0) == 0) config.virtualHosts[key.substr(6)] = value;
    }

    if (config.workerThreads == 0) throw std::runtime_error("worker_threads must be greater than 0");
    if (config.maxPendingConnections == 0) throw std::runtime_error("max_pending_connections must be greater than 0");
    if (config.maxHtaccessDepth == 0) throw std::runtime_error("max_htaccess_depth must be greater than 0");
    if (config.htaccessFilename.empty()) throw std::runtime_error("htaccess_filename cannot be empty");
    if (config.phpExtension.empty() || config.phpExtension.front() != '.') throw std::runtime_error("php_extension must start with dot");
    if (config.phpFastcgiPort == 0) throw std::runtime_error("php_fastcgi_port must be greater than 0");

    return config;
}

std::string ConfigLoader::trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

bool ConfigLoader::parseBool(const std::string& value) {
    std::string normalized = trim(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}
