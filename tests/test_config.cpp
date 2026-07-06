#include "Config.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;
int main() {
    fs::path configPath = fs::temp_directory_path() / "hydrogen_httpd_test_server.conf";
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
        "max_request_bytes = 4096\n"
        "max_body_bytes = 12345\n"
        "max_header_line_bytes = 1024\n"
        "max_headers = 20\n"
        "max_uri_bytes = 512\n"
        "max_method_bytes = 8\n"
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
        "auth_token.upload = upload-secret | upload\n"
        "auth_token.admin = admin-secret | upload,admin\n"
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
    assert(config.workerThreads == 3);
    assert(config.maxPendingConnections == 9);
    assert(config.denyHiddenFiles);
    assert(config.blockSensitiveFiles);
    assert(config.enableHtaccess);
    assert(config.enablePhp);
    assert(config.enableSql);
    assert(config.enableEndpointAuth);
    assert(config.authTokens.size() == 2);
    assert(config.authTokens[0].name == "upload");
    assert(config.authTokens[0].token == "upload-secret");
    assert(config.authTokens[0].scopes.size() == 1);
    assert(config.authTokens[0].scopes[0] == "upload");
    assert(config.authTokens[1].scopes.size() == 2);
    fs::remove(configPath);
    std::cout << "config tests passed\n";
    return 0;
}
