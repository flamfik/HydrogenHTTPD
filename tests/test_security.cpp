#include "Security.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;
int main() {
    auto base = fs::temp_directory_path() / "hydrogen_httpd_security_tests";
    fs::remove_all(base);
    fs::create_directories(base / "www");
    fs::create_directories(base / "secret");
    std::ofstream(base / "www" / "index.html") << "ok";
    std::ofstream(base / "secret" / "passwords.txt") << "secret";
    assert(security::isMethodAllowed("GET"));
    assert(security::isMethodAllowed("HEAD"));
    assert(security::isMethodAllowed("POST"));
    assert(!security::isMethodAllowed("TRACE"));
    auto index = security::resolveSafePath(base / "www", "/");
    assert(index.has_value());
    auto traversal = security::resolveSafePath(base / "www", "/../secret/passwords.txt");
    assert(!traversal.has_value());
    auto encoded = security::resolveSafePath(base / "www", "/%2e%2e/secret/passwords.txt");
    assert(!encoded.has_value());
    fs::remove_all(base);
    std::cout << "security tests passed\n";
    return 0;
}
