#pragma once
#include "Http.hpp"
#include <cstddef>
#include <string>

struct HttpParseLimits {
    std::size_t maxHeaderLineBytes = 4096;
    std::size_t maxHeaders = 100;
    std::size_t maxUriBytes = 2048;
    std::size_t maxMethodBytes = 16;
    std::size_t maxBodyBytes = 1024 * 1024;
};

struct HttpParseResult {
    bool ok = false;
    int status = 400;
    std::string reason = "Bad Request";
    std::string message = "400 Bad Request\n";
    HttpRequest request;
};

class StrictHttpParser {
public:
    static HttpParseResult parse(const std::string& raw, const HttpParseLimits& limits);

private:
    static bool isToken(const std::string& value);
    static std::string lowercase(std::string value);
};
