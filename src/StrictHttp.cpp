#include "StrictHttp.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace {
bool isCtl(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return u < 32 || u == 127;
}

bool isDigits(const std::string& value) {
    if (value.empty()) return false;
    for (unsigned char c : value) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

HttpParseResult fail(int status, std::string reason, std::string body) {
    HttpParseResult r;
    r.ok = false;
    r.status = status;
    r.reason = std::move(reason);
    r.message = std::move(body);
    return r;
}
}

std::string StrictHttpParser::lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool StrictHttpParser::isToken(const std::string& value) {
    if (value.empty()) return false;
    const std::string allowed = "!#$%&'*+-.^_`|~";
    for (char c : value) {
        unsigned char u = static_cast<unsigned char>(c);
        if (!(std::isalnum(u) || allowed.find(c) != std::string::npos)) return false;
    }
    return true;
}

HttpParseResult StrictHttpParser::parse(const std::string& raw, const HttpParseLimits& limits) {
    if (raw.empty()) return fail(400, "Bad Request", "400 Bad Request\n");

    auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return fail(400, "Bad Request", "400 Bad Request\n");
    std::size_t bodyOffset = headerEnd + 4;

    std::size_t start = 0;
    std::size_t end = raw.find("\r\n");
    if (end == std::string::npos || end > headerEnd) return fail(400, "Bad Request", "400 Bad Request\n");
    if (end > limits.maxHeaderLineBytes) return fail(431, "Request Header Fields Too Large", "431 Request Header Fields Too Large\n");

    std::string requestLine = raw.substr(0, end);
    std::istringstream first(requestLine);
    std::string extra;
    HttpParseResult result;
    first >> result.request.method >> result.request.target >> result.request.version;
    if (first >> extra) return fail(400, "Bad Request", "400 Bad Request\n");

    if (result.request.method.empty() || result.request.target.empty() || result.request.version.empty()) return fail(400, "Bad Request", "400 Bad Request\n");
    if (result.request.method.size() > limits.maxMethodBytes) return fail(405, "Method Not Allowed", "405 Method Not Allowed\n");
    if (!isToken(result.request.method)) return fail(400, "Bad Request", "400 Bad Request\n");
    if (result.request.target.size() > limits.maxUriBytes) return fail(414, "URI Too Long", "414 URI Too Long\n");
    if (result.request.version != "HTTP/1.1" && result.request.version != "HTTP/1.0") return fail(505, "HTTP Version Not Supported", "505 HTTP Version Not Supported\n");

    start = end + 2;
    std::size_t headerCount = 0;
    std::string contentLength;
    bool sawHost = false;

    while (true) {
        end = raw.find("\r\n", start);
        if (end == std::string::npos) return fail(400, "Bad Request", "400 Bad Request\n");
        if (end == start) break;
        if (end > headerEnd) return fail(400, "Bad Request", "400 Bad Request\n");
        if (end - start > limits.maxHeaderLineBytes) return fail(431, "Request Header Fields Too Large", "431 Request Header Fields Too Large\n");
        if (++headerCount > limits.maxHeaders) return fail(431, "Request Header Fields Too Large", "431 Request Header Fields Too Large\n");

        std::string line = raw.substr(start, end - start);
        if (!line.empty() && (line.front() == ' ' || line.front() == '\t')) return fail(400, "Bad Request", "400 Bad Request\n");

        auto colon = line.find(':');
        if (colon == std::string::npos) return fail(400, "Bad Request", "400 Bad Request\n");

        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();

        if (!isToken(name)) return fail(400, "Bad Request", "400 Bad Request\n");
        for (char c : value) {
            if (isCtl(c) && c != '\t') return fail(400, "Bad Request", "400 Bad Request\n");
        }

        name = lowercase(name);
        if (name == "host") {
            if (sawHost) return fail(400, "Bad Request", "400 Bad Request\n");
            sawHost = true;
        }
        if (name == "transfer-encoding") return fail(501, "Not Implemented", "501 Not Implemented\n");
        if (name == "content-length") {
            if (!contentLength.empty() && contentLength != value) return fail(400, "Bad Request", "400 Bad Request\n");
            contentLength = value;
        }

        result.request.headers[name] = value;
        start = end + 2;
    }

    if (result.request.version == "HTTP/1.1" && !sawHost) return fail(400, "Bad Request", "400 Bad Request\n");

    if (!contentLength.empty()) {
        if (!isDigits(contentLength)) return fail(400, "Bad Request", "400 Bad Request\n");
        try {
            auto len = static_cast<std::size_t>(std::stoull(contentLength));
            result.request.hasContentLength = true;
            result.request.contentLength = len;

            if (len > limits.maxBodyBytes) return fail(413, "Payload Too Large", "413 Payload Too Large\n");
            if (raw.size() < bodyOffset + len) return fail(400, "Bad Request", "400 Bad Request\n");
            result.request.body = raw.substr(bodyOffset, len);
        } catch (...) {
            return fail(400, "Bad Request", "400 Bad Request\n");
        }
    }

    result.ok = true;
    result.status = 200;
    result.reason = "OK";
    result.message.clear();
    return result;
}
