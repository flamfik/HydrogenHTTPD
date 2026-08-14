#include "Config.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;
int main() {
    fs::path configPath = fs::temp_directory_path() / "hydrogen_httpd_test_server.conf";
    fs::path secretsPath = fs::temp_directory_path() / "hydrogen_httpd_test_auth.tokens";
    std::ofstream(secretsPath)
        << "token.upload = sha256:e8f1c500156dad218cf39ff2398ec9a8aa18746c93dd3da01183a54b2a8ddba0 | upload | never\n"
        << "token.admin = sha256:16175223c8ddce5ace0493c948569c211b03c4c6bb3d3e484434999448cffe01 | upload,admin | 2000000000\n";

    std::ofstream(configPath) <<
        "port = 9090\n"
        "enable_tls = true\n"
        "tls_port = 9443\n"
        "public_https_port = 443\n"
        "tls_cert_file = certs/test.crt\n"
        "tls_key_file = certs/test.key\n"
        "force_https = true\n"
        "https_redirect_status = 308\n"
        "expose_server_header = false\n"
        "default_root = www\n"
        "audit_log = logs/audit.log\n"
        "audit_rotate_bytes = 4096\n"
        "audit_rotate_keep = 3\n"
        "auth_hot_reload = true\n"
        "auth_reload_interval_seconds = 0\n"
        "auth_secrets_file = hydrogen_httpd_test_auth.tokens\n"
        "max_request_bytes = 4096\n"
        "max_body_bytes = 12345\n"
        "max_header_line_bytes = 1024\n"
        "max_headers = 20\n"
        "max_uri_bytes = 512\n"
        "max_method_bytes = 8\n"
        "enable_rate_limiter = true\n"
        "rate_limiter_shards = 8\n"
        "io_threads = 2\n"
        "worker_thread_multiplier = 5\n"
        "listen_backlog = 2048\n"
        "tcp_no_delay = true\n"
        "tcp_keep_alive = true\n"
        "async_access_log = true\n"
        "enable_access_log = true\n"
        "access_log_sample_rate = 2\n"
        "access_log_queue_capacity = 1000\n"
        "access_log_flush_interval_ms = 20\n"
        "enable_static_cache = true\n"
        "static_cache_shards = 8\n"
        "static_cache_max_entries = 100\n"
        "static_cache_max_bytes = 1048576\n"
        "static_cache_max_file_bytes = 65536\n"
        "static_cache_revalidate_ms = 50\n"
        "static_cache_control = public, max-age=30\n"
        "rate_limit_per_minute = 7\n"
        "read_timeout_seconds = 2\n"
        "worker_threads = 3\n"
        "max_pending_connections = 9\n"
        "deny_hidden_files = true\n"
        "allow_dot_well_known = true\n"
        "block_sensitive_files = true\n"
        "enable_htaccess = true\n"
        "htaccess_filename = .htaccess\n"
        "max_htaccess_depth = 5\n"
        "enable_php = true\n"
        "php_extension = .php\n"
        "php_fastcgi_host = 127.0.0.1\n"
        "php_fastcgi_port = 9000\n"
        "enable_sql = true\n"
        "sqlite_database = sql/test.db\n"
        "enable_endpoint_auth = true\n"
        "enable_admin_status = true\n"
        "admin_status_endpoint = /__hydrogen/admin/status\n"
        "vhost.localhost = www\n";
    auto config = ConfigLoader::load(configPath);
    assert(config.port == 9090);
    assert(config.enableTls);
    assert(config.tlsPort == 9443);
    assert(config.publicHttpsPort == 443);
    assert(config.forceHttps);
    assert(config.httpsRedirectStatus == 308);
    assert(!config.exposeServerHeader);
    assert(config.maxBodyBytes == 12345);
    assert(config.maxHeaderLineBytes == 1024);
    assert(config.maxHeaders == 20);
    assert(config.maxUriBytes == 512);
    assert(config.maxMethodBytes == 8);
    assert(config.enableRateLimiter);
    assert(config.rateLimiterShards == 8);
    assert(config.ioThreads == 2);
    assert(config.workerThreadMultiplier == 5);
    assert(config.listenBacklog == 2048);
    assert(config.tcpNoDelay);
    assert(config.tcpKeepAlive);
    assert(config.asyncAccessLog);
    assert(config.enableAccessLog);
    assert(config.accessLogSampleRate == 2);
    assert(config.accessLogQueueCapacity == 1000);
    assert(config.accessLogFlushIntervalMs == 20);
    assert(config.enableStaticCache);
    assert(config.staticCacheShards == 8);
    assert(config.staticCacheMaxEntries == 100);
    assert(config.staticCacheMaxBytes == 1048576);
    assert(config.staticCacheMaxFileBytes == 65536);
    assert(config.staticCacheRevalidateMs == 50);
    assert(config.staticCacheControl == "public, max-age=30");
    assert(config.workerThreads == 3);
    assert(config.maxPendingConnections == 9);
    assert(config.denyHiddenFiles);
    assert(config.blockSensitiveFiles);
    assert(config.enableHtaccess);
    assert(config.enablePhp);
    assert(config.enableSql);
    assert(config.enableEndpointAuth);
    assert(config.authTokens.empty());
    assert(config.authSecretsFile == secretsPath);
    assert(config.authHotReload);
    assert(config.authReloadIntervalSeconds == 0);
    assert(config.auditRotateBytes == 4096);
    assert(config.auditRotateKeep == 3);

    auto externalTokens = Auth::loadSecretsFile(config.authSecretsFile);
    assert(externalTokens.size() == 2);
    assert(externalTokens[0].name == "upload");
    assert(externalTokens[0].plaintextToken.empty());
    assert(externalTokens[0].sha256Hex == "e8f1c500156dad218cf39ff2398ec9a8aa18746c93dd3da01183a54b2a8ddba0");
    assert(externalTokens[0].scopes.size() == 1);
    assert(externalTokens[0].scopes[0] == "upload");
    assert(externalTokens[1].scopes.size() == 2);
    assert(externalTokens[1].expiresAtEpoch == 2000000000);
    assert(config.enableAdminStatus);
    assert(config.adminStatusEndpoint == "/__hydrogen/admin/status");
    fs::remove(configPath);
    fs::remove(secretsPath);
    std::cout << "config tests passed\n";
    return 0;
}
