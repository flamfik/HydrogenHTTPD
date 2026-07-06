#include "Htaccess.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    auto base = fs::temp_directory_path() / "hydrogen_httpd_htaccess_tests";
    fs::remove_all(base);
    fs::create_directories(base / "www" / "private" / "public");

    std::ofstream(base / "www" / ".htaccess") << "Header set X-Root ok\nOptions -Indexes\n";
    std::ofstream(base / "www" / "private" / ".htaccess") << "Require all denied\nHeader set X-Zone private\n";
    std::ofstream(base / "www" / "private" / "public" / ".htaccess") << "Require all granted\nHeader set X-Zone public\n";
    std::ofstream(base / "www" / "private" / "secret.txt") << "secret";
    std::ofstream(base / "www" / "private" / "public" / "index.html") << "public";

    auto denied = Htaccess::evaluate(base / "www", base / "www" / "private" / "secret.txt", ".htaccess", 8);
    assert(denied.denyAll);
    assert(denied.responseHeaders.at("X-Zone") == "private");
    assert(denied.responseHeaders.at("X-Root") == "ok");

    auto granted = Htaccess::evaluate(base / "www", base / "www" / "private" / "public" / "index.html", ".htaccess", 8);
    assert(!granted.denyAll);
    assert(granted.responseHeaders.at("X-Zone") == "public");

    fs::remove_all(base);
    std::cout << "htaccess tests passed\n";
    return 0;
}
