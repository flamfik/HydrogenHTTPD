#include "Config.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace {
std::vector<std::string> splitScopes(std::string text) {
    std::vector<std::string> scopes;
    std::stringstream ss(text);
    std::string item;

    while (std::getline(ss, item, ',')) {
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) item.erase(item.begin());
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) item.pop_back();
        std::transform(item.begin(), item.end(), item.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (!item.empty()) scopes.push_back(item);
    }

    return scopes;
}

AuthTokenRule parseAuthTokenRule(const std::string& name, const std::string& value) {
    auto sep = value.find('|');
    if (sep == std::string::npos) {
        AuthTokenRule rule;
        rule.name = name;
        rule.token = value;
        rule.scopes = {"upload"};
        return rule;
    }

    AuthTokenRule rule;
    rule.name = name;
    rule.token = value.substr(0, sep);
    rule.scopes = splitScopes(value.substr(sep + 1));

    while (!rule.token.empty() && std::isspace(static_cast<unsigned char>(rule.token.front()))) rule.token.erase(rule.token.begin());
    while (!rule.token.empty() && std::isspace(static_cast<unsigned char>(rule.token.back()))) rule.token.pop_back();

    return rule;
}
}

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
        else if (key == "public_https_port") config.publicHttpsPort = static_cast<unsigned short>(std::stoi(value));
        else if (key == "tls_cert_file") config.tlsCertFile = value;
        else if (key == "tls_key_file") config.tlsKeyFile = value;
        else if (key == "force_https") config.forceHttps = parseBool(value);
        else if (key == "https_redirect_status") config.httpsRedirectStatus = std::stoi(value);
        else if (key == "expose_server_header") config.exposeServerHeader = parseBool(value);
        else if (key == "default_root") config.defaultRoot = value;
        else if (key == "access_log") config.accessLog = value;
        else if (key == "error_log") config.errorLog = value;
        else if (key == "audit_log") config.auditLog = value;
        else if (key == "max_request_bytes") config.maxRequestBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_body_bytes") config.maxBodyBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_header_line_bytes") config.maxHeaderLineBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_headers") config.maxHeaders = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_uri_bytes") config.maxUriBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_method_bytes") config.maxMethodBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "rate_limit_per_minute") config.rateLimitPerMinute = static_cast<std::size_t>(std::stoull(value));
        else if (key == "read_timeout_seconds") config.readTimeoutSeconds = static_cast<unsigned int>(std::stoul(value));
        else if (key == "worker_threads") config.workerThreads = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_pending_connections") config.maxPendingConnections = static_cast<std::size_t>(std::stoull(value));
        else if (key == "deny_hidden_files") config.denyHiddenFiles = parseBool(value);
        else if (key == "allow_dot_well_known") config.allowDotWellKnown = parseBool(value);
        else if (key == "block_sensitive_files") config.blockSensitiveFiles = parseBool(value);
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
        else if (key == "enable_uploads") config.enableUploads = parseBool(value);
        else if (key == "upload_endpoint") config.uploadEndpoint = value;
        else if (key == "upload_directory") config.uploadDirectory = value;
        else if (key == "upload_spool_directory") config.uploadSpoolDirectory = value;
        else if (key == "max_multipart_parts") config.maxMultipartParts = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_upload_file_bytes") config.maxUploadFileBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_upload_field_bytes") config.maxUploadFieldBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "enable_endpoint_auth") config.enableEndpointAuth = parseBool(value);
        else if (key == "auth_bearer_token") config.authBearerToken = value;
        else if (key.rfind("auth_token.", 0) == 0) config.authTokens.push_back(parseAuthTokenRule(key.substr(11), value));
        else if (key.rfind("vhost.", 0) == 0) config.virtualHosts[key.substr(6)] = value;
    }

    if (config.workerThreads == 0) throw std::runtime_error("worker_threads must be greater than 0");
    if (config.maxPendingConnections == 0) throw std::runtime_error("max_pending_connections must be greater than 0");
    if (config.maxHtaccessDepth == 0) throw std::runtime_error("max_htaccess_depth must be greater than 0");
    if (config.htaccessFilename.empty()) throw std::runtime_error("htaccess_filename cannot be empty");
    if (config.phpExtension.empty() || config.phpExtension.front() != '.') throw std::runtime_error("php_extension must start with dot");
    if (config.phpFastcgiPort == 0) throw std::runtime_error("php_fastcgi_port must be greater than 0");
    if (config.maxRequestBytes < 1024) throw std::runtime_error("max_request_bytes is too small");
    if (config.maxBodyBytes > 64 * 1024 * 1024) throw std::runtime_error("max_body_bytes is too large for this baseline");
    if (config.maxHeaderLineBytes < 256) throw std::runtime_error("max_header_line_bytes is too small");
    if (config.maxHeaders == 0) throw std::runtime_error("max_headers must be greater than 0");
    if (config.maxUriBytes == 0) throw std::runtime_error("max_uri_bytes must be greater than 0");
    if (config.httpsRedirectStatus != 301 && config.httpsRedirectStatus != 308) throw std::runtime_error("https_redirect_status must be 301 or 308");
    if (config.uploadEndpoint.empty() || config.uploadEndpoint.front() != '/') throw std::runtime_error("upload_endpoint must start with /");
    if (config.maxMultipartParts == 0) throw std::runtime_error("max_multipart_parts must be greater than 0");
    if (config.enableUploads) {
        if (config.maxUploadFileBytes == 0 || config.maxUploadFileBytes > config.maxBodyBytes) throw std::runtime_error("max_upload_file_bytes must be greater than 0 and <= max_body_bytes");
        if (config.maxUploadFieldBytes > config.maxBodyBytes) throw std::runtime_error("max_upload_field_bytes must be <= max_body_bytes");
    }
    if (config.enableEndpointAuth && config.authBearerToken.empty() && config.authTokens.empty()) throw std::runtime_error("auth_bearer_token or auth_token.* cannot be empty when endpoint auth is enabled");
    for (const auto& token : config.authTokens) {
        if (token.name.empty()) throw std::runtime_error("auth_token name cannot be empty");
        if (token.token.empty()) throw std::runtime_error("auth_token value cannot be empty");
        if (token.scopes.empty()) throw std::runtime_error("auth_token scopes cannot be empty");
    }

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
