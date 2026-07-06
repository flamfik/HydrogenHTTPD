#pragma once
#include <cstddef>
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
    bool headOnly = false;
    bool tls = false;
    bool exposeServerHeader = false;
    std::unordered_map<std::string, std::string> extraHeaders;

    std::string toString() const {
        std::string response;
        response += "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
        if (exposeServerHeader) response += "Server: HydrogenHttpd\r\n";
        response += "Connection: close\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        response += "Content-Type: " + contentType + "\r\n";
        response += "Cache-Control: no-store\r\n";
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
        if (!headOnly) response += body;
        return response;
    }
};
