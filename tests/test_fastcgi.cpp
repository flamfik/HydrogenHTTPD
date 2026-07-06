#include "FastCgiClient.hpp"
#include <cassert>
#include <iostream>
#include <string>

int main() {
    std::unordered_map<std::string, std::string> params{
        {"SCRIPT_FILENAME", "/var/www/index.php"},
        {"REQUEST_METHOD", "GET"}
    };

    auto encoded = FastCgiClient::encodeNameValuePairs(params);
    assert(!encoded.empty());

    std::cout << "fastcgi tests passed\n";
    return 0;
}
