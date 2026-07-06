#pragma once
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
};

struct HttpResponse {
    int status = 200;
    std::string reason = "OK";
    std::string contentType = "text/plain; charset=utf-8";
    std::string body;
    bool headOnly = false;
    bool tls = false;
    std::unordered_map<std::string, std::string> extraHeaders;

    std::string toString() const {
        std::string response;
        response += "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
        response += "Server: HydrogenHttpd\r\n";
        response += "Connection: close\r\n";
        response += "Content-Length: " + std::to_string(headOnly ? 0 : body.size()) + "\r\n";
        response += "Content-Type: " + contentType + "\r\n";
        response += "X-Content-Type-Options: nosniff\r\n";
        response += "X-Frame-Options: DENY\r\n";
        response += "Referrer-Policy: no-referrer\r\n";
        response += "Permissions-Policy: geolocation=(), microphone=(), camera=()\r\n";
        response += "Content-Security-Policy: default-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'self';\r\n";
        if (tls) response += "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n";

        for (const auto& [name, value] : extraHeaders) {
            response += name + ": " + value + "\r\n";
        }

        response += "\r\n";
        if (!headOnly) response += body;
        return response;
    }
};
