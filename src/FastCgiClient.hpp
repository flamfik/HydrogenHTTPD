#pragma once
#include <boost/asio.hpp>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct FastCgiResponse {
    int status = 200;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct FastCgiRequest {
    std::filesystem::path scriptFilename;
    std::string requestMethod;
    std::string requestUri;
    std::string queryString;
    std::string documentRoot;
    std::string serverName = "localhost";
    std::string serverProtocol = "HTTP/1.1";
    std::string remoteAddr = "127.0.0.1";
};

class FastCgiClient {
public:
    FastCgiClient(
        boost::asio::io_context& io,
        std::string host,
        unsigned short port,
        unsigned int connectTimeoutSeconds,
        unsigned int readTimeoutSeconds
    );

    FastCgiResponse execute(const FastCgiRequest& request);

    static std::vector<unsigned char> encodeNameValuePairs(
        const std::unordered_map<std::string, std::string>& params
    );

private:
    std::vector<unsigned char> buildBeginRequest() const;
    std::vector<unsigned char> buildParams(const std::unordered_map<std::string, std::string>& params) const;
    std::vector<unsigned char> buildStdinEmpty() const;
    static void appendRecord(std::vector<unsigned char>& out, std::uint8_t type, std::uint16_t requestId, const std::vector<unsigned char>& content);
    static FastCgiResponse parseCgiResponse(const std::string& raw);

private:
    boost::asio::io_context& io_;
    std::string host_;
    unsigned short port_;
    unsigned int connectTimeoutSeconds_;
    unsigned int readTimeoutSeconds_;
};
