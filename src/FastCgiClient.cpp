#include "FastCgiClient.hpp"
#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif

namespace {
constexpr std::uint8_t FCGI_VERSION_1 = 1;
constexpr std::uint8_t FCGI_BEGIN_REQUEST = 1;
constexpr std::uint8_t FCGI_ABORT_REQUEST = 2;
constexpr std::uint8_t FCGI_END_REQUEST = 3;
constexpr std::uint8_t FCGI_PARAMS = 4;
constexpr std::uint8_t FCGI_STDIN = 5;
constexpr std::uint8_t FCGI_STDOUT = 6;
constexpr std::uint8_t FCGI_STDERR = 7;
constexpr std::uint16_t FCGI_RESPONDER = 1;
constexpr std::uint16_t REQUEST_ID = 1;

template <typename NativeHandle>
void setReceiveTimeout(NativeHandle handle, unsigned int seconds) {
#if defined(_WIN32)
    DWORD timeout = static_cast<DWORD>(seconds * 1000);
    setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout{};
    timeout.tv_sec = static_cast<long>(seconds);
    timeout.tv_usec = 0;
    setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}

void appendLength(std::vector<unsigned char>& out, std::size_t len) {
    if (len < 128) {
        out.push_back(static_cast<unsigned char>(len));
    } else {
        out.push_back(static_cast<unsigned char>((len >> 24) | 0x80));
        out.push_back(static_cast<unsigned char>(len >> 16));
        out.push_back(static_cast<unsigned char>(len >> 8));
        out.push_back(static_cast<unsigned char>(len));
    }
}

std::string trimCrlf(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}
}

FastCgiClient::FastCgiClient(
    boost::asio::io_context& io,
    std::string host,
    unsigned short port,
    unsigned int connectTimeoutSeconds,
    unsigned int readTimeoutSeconds
) : io_(io),
    host_(std::move(host)),
    port_(port),
    connectTimeoutSeconds_(connectTimeoutSeconds),
    readTimeoutSeconds_(readTimeoutSeconds) {}

std::vector<unsigned char> FastCgiClient::encodeNameValuePairs(
    const std::unordered_map<std::string, std::string>& params
) {
    std::vector<unsigned char> out;
    for (const auto& [name, value] : params) {
        appendLength(out, name.size());
        appendLength(out, value.size());
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), value.begin(), value.end());
    }
    return out;
}

void FastCgiClient::appendRecord(
    std::vector<unsigned char>& out,
    std::uint8_t type,
    std::uint16_t requestId,
    const std::vector<unsigned char>& content
) {
    std::size_t padding = (8 - (content.size() % 8)) % 8;
    out.push_back(FCGI_VERSION_1);
    out.push_back(type);
    out.push_back(static_cast<unsigned char>(requestId >> 8));
    out.push_back(static_cast<unsigned char>(requestId));
    out.push_back(static_cast<unsigned char>(content.size() >> 8));
    out.push_back(static_cast<unsigned char>(content.size()));
    out.push_back(static_cast<unsigned char>(padding));
    out.push_back(0);
    out.insert(out.end(), content.begin(), content.end());
    out.insert(out.end(), padding, 0);
}

std::vector<unsigned char> FastCgiClient::buildBeginRequest() const {
    std::vector<unsigned char> payload;
    payload.push_back(static_cast<unsigned char>(FCGI_RESPONDER >> 8));
    payload.push_back(static_cast<unsigned char>(FCGI_RESPONDER));
    payload.push_back(0); // flags: close connection after request
    payload.insert(payload.end(), 5, 0);

    std::vector<unsigned char> out;
    appendRecord(out, FCGI_BEGIN_REQUEST, REQUEST_ID, payload);
    return out;
}

std::vector<unsigned char> FastCgiClient::buildParams(const std::unordered_map<std::string, std::string>& params) const {
    std::vector<unsigned char> encoded = encodeNameValuePairs(params);
    std::vector<unsigned char> out;

    constexpr std::size_t chunkSize = 65535;
    for (std::size_t offset = 0; offset < encoded.size(); offset += chunkSize) {
        auto end = std::min(encoded.size(), offset + chunkSize);
        std::vector<unsigned char> chunk(encoded.begin() + static_cast<std::ptrdiff_t>(offset),
                                         encoded.begin() + static_cast<std::ptrdiff_t>(end));
        appendRecord(out, FCGI_PARAMS, REQUEST_ID, chunk);
    }

    appendRecord(out, FCGI_PARAMS, REQUEST_ID, {});
    return out;
}

std::vector<unsigned char> FastCgiClient::buildStdinEmpty() const {
    std::vector<unsigned char> out;
    appendRecord(out, FCGI_STDIN, REQUEST_ID, {});
    return out;
}

FastCgiResponse FastCgiClient::execute(const FastCgiRequest& request) {
    using boost::asio::ip::tcp;

    tcp::resolver resolver(io_);
    auto endpoints = resolver.resolve(host_, std::to_string(port_));

    tcp::socket socket(io_);
    boost::asio::connect(socket, endpoints);
    setReceiveTimeout(socket.native_handle(), readTimeoutSeconds_);

    std::string script = request.scriptFilename.string();
    std::replace(script.begin(), script.end(), '\\', '/');

    std::unordered_map<std::string, std::string> params{
        {"GATEWAY_INTERFACE", "CGI/1.1"},
        {"REQUEST_METHOD", request.requestMethod},
        {"SCRIPT_FILENAME", script},
        {"SCRIPT_NAME", request.requestUri},
        {"REQUEST_URI", request.requestUri},
        {"QUERY_STRING", request.queryString},
        {"DOCUMENT_ROOT", request.documentRoot},
        {"SERVER_SOFTWARE", "HydrogenHttpd"},
        {"SERVER_PROTOCOL", request.serverProtocol},
        {"SERVER_NAME", request.serverName},
        {"REMOTE_ADDR", request.remoteAddr},
        {"REDIRECT_STATUS", "200"}
    };

    std::vector<unsigned char> wire;
    auto begin = buildBeginRequest();
    auto par = buildParams(params);
    auto in = buildStdinEmpty();
    wire.insert(wire.end(), begin.begin(), begin.end());
    wire.insert(wire.end(), par.begin(), par.end());
    wire.insert(wire.end(), in.begin(), in.end());

    boost::asio::write(socket, boost::asio::buffer(wire));

    std::string stdoutData;
    std::string stderrData;

    while (true) {
        std::array<unsigned char, 8> header{};
        boost::system::error_code ec;
        std::size_t n = boost::asio::read(socket, boost::asio::buffer(header), ec);
        if (ec) break;
        if (n != header.size()) break;

        auto type = header[1];
        auto requestId = static_cast<std::uint16_t>((header[2] << 8) | header[3]);
        auto contentLength = static_cast<std::uint16_t>((header[4] << 8) | header[5]);
        auto paddingLength = header[6];

        if (requestId != REQUEST_ID && type != FCGI_END_REQUEST) {
            throw std::runtime_error("FastCGI request id mismatch");
        }

        std::vector<unsigned char> content(contentLength);
        if (contentLength > 0) {
            boost::asio::read(socket, boost::asio::buffer(content));
        }

        if (paddingLength > 0) {
            std::vector<unsigned char> padding(paddingLength);
            boost::asio::read(socket, boost::asio::buffer(padding));
        }

        if (type == FCGI_STDOUT) {
            stdoutData.append(reinterpret_cast<const char*>(content.data()), content.size());
        } else if (type == FCGI_STDERR) {
            stderrData.append(reinterpret_cast<const char*>(content.data()), content.size());
        } else if (type == FCGI_END_REQUEST) {
            break;
        }
    }

    if (!stderrData.empty() && stdoutData.empty()) {
        throw std::runtime_error("FastCGI stderr: " + stderrData);
    }

    return parseCgiResponse(stdoutData);
}

FastCgiResponse FastCgiClient::parseCgiResponse(const std::string& raw) {
    FastCgiResponse response;

    auto split = raw.find("\r\n\r\n");
    std::size_t sepLen = 4;
    if (split == std::string::npos) {
        split = raw.find("\n\n");
        sepLen = 2;
    }

    if (split == std::string::npos) {
        response.body = raw;
        return response;
    }

    std::string headers = raw.substr(0, split);
    response.body = raw.substr(split + sepLen);

    std::istringstream ss(headers);
    std::string line;
    while (std::getline(ss, line)) {
        line = trimCrlf(line);
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());

        if (name == "Status") {
            try {
                response.status = std::stoi(value);
            } catch (...) {
                response.status = 200;
            }
        } else {
            response.headers[name] = value;
        }
    }

    return response;
}
