#include "Config.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <unordered_set>
#include <thread>


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
        else if (key == "audit_rotate_bytes") config.auditRotateBytes = static_cast<std::uintmax_t>(std::stoull(value));
        else if (key == "audit_rotate_keep") config.auditRotateKeep = static_cast<std::size_t>(std::stoull(value));
        else if (key == "auth_secrets_file") config.authSecretsFile = value;
        else if (key == "auth_hot_reload") config.authHotReload = parseBool(value);
        else if (key == "auth_reload_interval_seconds") config.authReloadIntervalSeconds = static_cast<unsigned int>(std::stoul(value));
        else if (key == "max_request_bytes") config.maxRequestBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_body_bytes") config.maxBodyBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_header_line_bytes") config.maxHeaderLineBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_headers") config.maxHeaders = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_uri_bytes") config.maxUriBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_method_bytes") config.maxMethodBytes = static_cast<std::size_t>(std::stoull(value));
        else if (key == "enable_rate_limiter") config.enableRateLimiter = parseBool(value);
        else if (key == "rate_limit_per_minute") config.rateLimitPerMinute = static_cast<std::size_t>(std::stoull(value));
        else if (key == "rate_limiter_shards") config.rateLimiterShards = static_cast<std::size_t>(std::stoull(value));
        else if (key == "read_timeout_seconds") config.readTimeoutSeconds = static_cast<unsigned int>(std::stoul(value));
        else if (key == "io_threads") config.ioThreads = static_cast<std::size_t>(std::stoull(value));
        else if (key == "worker_threads") config.workerThreads = static_cast<std::size_t>(std::stoull(value));
        else if (key == "worker_thread_multiplier") config.workerThreadMultiplier = static_cast<std::size_t>(std::stoull(value));
        else if (key == "max_pending_connections") config.maxPendingConnections = static_cast<std::size_t>(std::stoull(value));
        else if (key == "listen_backlog") config.listenBacklog = std::stoi(value);
        else if (key == "tcp_no_delay") config.tcpNoDelay = parseBool(value);
        else if (key == "tcp_keep_alive") config.tcpKeepAlive = parseBool(value);
        else if (key == "socket_receive_buffer_bytes") config.socketReceiveBufferBytes = std::stoi(value);
        else if (key == "socket_send_buffer_bytes") config.socketSendBufferBytes = std::stoi(value);
        else if (key == "async_access_log") config.asyncAccessLog = parseBool(value);
        else if (key == "enable_access_log") config.enableAccessLog = parseBool(value);
        else if (key == "access_log_sample_rate") config.accessLogSampleRate = static_cast<std::size_t>(std::stoull(value));
        else if (key == "access_log_queue_capacity") config.accessLogQueueCapacity = static_cast<std::size_t>(std::stoull(value));
        else if (key == "access_log_flush_interval_ms") config.accessLogFlushIntervalMs = static_cast<unsigned int>(std::stoul(value));
        else if (key == "enable_static_cache") config.enableStaticCache = parseBool(value);
        else if (key == "static_cache_shards") config.staticCacheShards = static_cast<std::size_t>(std::stoull(value));
        else if (key == "static_cache_max_entries") config.staticCacheMaxEntries = static_cast<std::size_t>(std::stoull(value));
        else if (key == "static_cache_max_bytes") config.staticCacheMaxBytes = static_cast<std::uintmax_t>(std::stoull(value));
        else if (key == "static_cache_max_file_bytes") config.staticCacheMaxFileBytes = static_cast<std::uintmax_t>(std::stoull(value));
        else if (key == "static_cache_revalidate_ms") config.staticCacheRevalidateMs = static_cast<unsigned int>(std::stoul(value));
        else if (key == "static_cache_control") config.staticCacheControl = value;
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
        else if (key.rfind("auth_token.", 0) == 0) config.authTokens.push_back(Auth::parseTokenRule(key.substr(11), value, false));
        else if (key == "enable_admin_status") config.enableAdminStatus = parseBool(value);
        else if (key == "admin_status_endpoint") config.adminStatusEndpoint = value;
        else if (key.rfind("vhost.", 0) == 0) config.virtualHosts[key.substr(6)] = value;
    }

    const auto configBase = std::filesystem::absolute(path).parent_path();
    auto resolvePath = [&configBase](std::filesystem::path& value) {
        if (!value.empty() && value.is_relative()) value = (configBase / value).lexically_normal();
    };

    resolvePath(config.tlsCertFile);
    resolvePath(config.tlsKeyFile);
    resolvePath(config.defaultRoot);
    resolvePath(config.accessLog);
    resolvePath(config.errorLog);
    resolvePath(config.auditLog);
    resolvePath(config.authSecretsFile);
    resolvePath(config.sqliteDatabase);
    resolvePath(config.uploadDirectory);
    resolvePath(config.uploadSpoolDirectory);
    for (auto& [host, root] : config.virtualHosts) {
        (void)host;
        resolvePath(root);
    }

    if (!config.authSecretsFile.empty()) {
        (void)Auth::loadSecretsFile(config.authSecretsFile);
    }

    if (config.ioThreads == 0) throw std::runtime_error("io_threads must be greater than 0");
    if (config.workerThreadMultiplier == 0) throw std::runtime_error("worker_thread_multiplier must be greater than 0");
    if (config.workerThreads == 0) {
        const auto hardware = std::max(1u, std::thread::hardware_concurrency());
        config.workerThreads = std::max<std::size_t>(4, static_cast<std::size_t>(hardware) * config.workerThreadMultiplier);
    }
    if (config.maxPendingConnections == 0) throw std::runtime_error("max_pending_connections must be greater than 0");
    if (config.listenBacklog <= 0) throw std::runtime_error("listen_backlog must be greater than 0");
    if (config.rateLimiterShards == 0) throw std::runtime_error("rate_limiter_shards must be greater than 0");
    if (config.accessLogSampleRate == 0) throw std::runtime_error("access_log_sample_rate must be greater than 0");
    if (config.accessLogQueueCapacity == 0) throw std::runtime_error("access_log_queue_capacity must be greater than 0");
    if (config.staticCacheShards == 0) throw std::runtime_error("static_cache_shards must be greater than 0");
    if (config.enableStaticCache && (config.staticCacheMaxEntries == 0 || config.staticCacheMaxBytes == 0)) throw std::runtime_error("static cache limits must be greater than 0 when enabled");
    if (config.staticCacheMaxFileBytes > config.staticCacheMaxBytes) throw std::runtime_error("static_cache_max_file_bytes must be <= static_cache_max_bytes");
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
    if (config.enableEndpointAuth && config.authBearerToken.empty() && config.authTokens.empty() && config.authSecretsFile.empty()) throw std::runtime_error("auth_bearer_token, auth_token.*, or auth_secrets_file is required when endpoint auth is enabled");
    std::unordered_set<std::string> authTokenNames;
    for (const auto& token : config.authTokens) {
        if (token.name.empty()) throw std::runtime_error("auth_token name cannot be empty");
        if (token.plaintextToken.empty() && token.sha256Hex.empty()) throw std::runtime_error("auth_token value cannot be empty");
        if (token.scopes.empty()) throw std::runtime_error("auth_token scopes cannot be empty");
        if (!authTokenNames.insert(token.name).second) throw std::runtime_error("duplicate auth_token name: " + token.name);
    }
    if (config.auditRotateBytes > 0 && config.auditRotateKeep == 0) throw std::runtime_error("audit_rotate_keep must be greater than 0 when rotation is enabled");
    if (config.authReloadIntervalSeconds > 86400) throw std::runtime_error("auth_reload_interval_seconds is unreasonably large");
    if (config.adminStatusEndpoint.empty() || config.adminStatusEndpoint.front() != '/') throw std::runtime_error("admin_status_endpoint must start with /");
    if (config.enableAdminStatus && !config.enableEndpointAuth) throw std::runtime_error("enable_admin_status requires enable_endpoint_auth");

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
