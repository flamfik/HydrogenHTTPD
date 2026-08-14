#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    bool hasContentLength = false;
    std::size_t contentLength = 0;
};

struct HttpResponse {
    int status = 200;
    std::string reason = "OK";
    std::string contentType = "text/plain; charset=utf-8";
    std::string body;
    std::shared_ptr<const std::string> sharedBody;
    bool headOnly = false;
    bool tls = false;
    bool exposeServerHeader = false;
    std::string cacheControl = "no-store";
    std::unordered_map<std::string, std::string> extraHeaders;

    std::size_t bodySize() const noexcept {
        return sharedBody ? sharedBody->size() : body.size();
    }

    const std::string& bodyData() const noexcept {
        return sharedBody ? *sharedBody : body;
    }

    std::string headersToString() const {
        std::string response;
        response.reserve(768);
        response += "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
        if (exposeServerHeader) response += "Server: HydrogenHttpd\r\n";
        response += "Connection: close\r\n";
        response += "Content-Length: " + std::to_string(bodySize()) + "\r\n";
        response += "Content-Type: " + contentType + "\r\n";
        if (!cacheControl.empty()) response += "Cache-Control: " + cacheControl + "\r\n";
        response += "X-Content-Type-Options: nosniff\r\n";
        response += "X-Frame-Options: DENY\r\n";
        response += "X-XSS-Protection: 0\r\n";
        response += "Referrer-Policy: strict-origin-when-cross-origin\r\n";
        response += "Permissions-Policy: geolocation=(), microphone=(), camera=(), payment=(), usb=(), interest-cohort=()\r\n";
        response += "Cross-Origin-Opener-Policy: same-origin\r\n";
        response += "Cross-Origin-Resource-Policy: same-origin\r\n";
        response += "Content-Security-Policy: default-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'self'; form-action 'self';\r\n";
        if (tls) response += "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n";

        for (const auto& [name, value] : extraHeaders) response += name + ": " + value + "\r\n";
        response += "\r\n";
        return response;
    }

    std::string toString() const {
        std::string response = headersToString();
        if (!headOnly) response += bodyData();
        return response;
    }
};
