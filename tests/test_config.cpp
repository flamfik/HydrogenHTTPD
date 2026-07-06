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
        "tls_cert_file = certs/test.crt\n"
        "tls_key_file = certs/test.key\n"
        "default_root = www\n"
        "max_request_bytes = 4096\n"
        "rate_limit_per_minute = 7\n"
        "read_timeout_seconds = 2\n"
        "worker_threads = 3\n"
        "max_pending_connections = 9\n"
        "enable_htaccess = true\n"
        "htaccess_filename = .htaccess\n"
        "max_htaccess_depth = 5\n"
        "enable_php = true\n"
        "php_extension = .php\n"
        "php_fastcgi_host = 127.0.0.1\n"
        "php_fastcgi_port = 9000\n"
        "enable_sql = true\n"
        "sqlite_database = sql/test.db\n"
        "vhost.localhost = www\n";
    auto config = ConfigLoader::load(configPath);
    assert(config.port == 9090);
    assert(config.enableTls);
    assert(config.tlsPort == 9443);
    assert(config.workerThreads == 3);
    assert(config.maxPendingConnections == 9);
    assert(config.enableHtaccess);
    assert(config.htaccessFilename == ".htaccess");
    assert(config.maxHtaccessDepth == 5);
    assert(config.enablePhp);
    assert(config.phpExtension == ".php");
    assert(config.phpFastcgiHost == "127.0.0.1");
    assert(config.phpFastcgiPort == 9000);
    assert(config.enableSql);
    assert(config.sqliteDatabase == "sql/test.db");
    fs::remove(configPath);
    std::cout << "config tests passed\n";
    return 0;
}
