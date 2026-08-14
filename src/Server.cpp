#include "Server.hpp"
#include "FastCgiClient.hpp"
#include "Multipart.hpp"
#include "Htaccess.hpp"
#include "Http.hpp"
#include "Security.hpp"
#include <algorithm>
#include <array>
#include <iomanip>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#ifndef HYDROGENHTTPD_VERSION
#define HYDROGENHTTPD_VERSION "dev"
#endif

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif

using boost::asio::ip::tcp;
namespace fs = std::filesystem;

namespace {
std::string lowercaseLocal(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trimHeaderValue(std::string value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
    return value;
}

bool parseHeaderContentLength(const std::string& headerPart, std::size_t& out) {
    std::size_t start = headerPart.find("\r\n");
    if (start == std::string::npos) return false;
    start += 2;

    bool found = false;
    std::size_t value = 0;

    while (true) {
        std::size_t end = headerPart.find("\r\n", start);
        if (end == std::string::npos || end == start) break;

        std::string line = headerPart.substr(start, end - start);
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = lowercaseLocal(line.substr(0, colon));
            if (name == "content-length") {
                std::string text = trimHeaderValue(line.substr(colon + 1));
                if (text.empty()) return false;
                for (unsigned char c : text) {
                    if (!std::isdigit(c)) return false;
                }

                try {
                    std::size_t parsed = static_cast<std::size_t>(std::stoull(text));
                    if (found && parsed != value) return false;
                    value = parsed;
                    found = true;
                } catch (...) {
                    return false;
                }
            }
        }

        start = end + 2;
    }

    if (!found) return false;
    out = value;
    return true;
}

template <typename SyncReadStream>
std::string readRequestWithBody(SyncReadStream& stream, const ServerConfig& config) {
    std::string data;
    data.reserve(std::min<std::size_t>(config.maxRequestBytes, 4096));
    std::array<char, 512> buffer{};

    while (data.find("\r\n\r\n") == std::string::npos) {
        if (data.size() >= config.maxRequestBytes) throw std::runtime_error("request headers too large");
        boost::system::error_code ec;
        std::size_t remaining = config.maxRequestBytes - data.size();
        std::size_t chunkSize = std::min(buffer.size(), remaining);
        std::size_t bytes = stream.read_some(boost::asio::buffer(buffer.data(), chunkSize), ec);
        if (ec == boost::asio::error::eof) break;
        if (ec) throw std::runtime_error("failed to read request headers: " + ec.message());
        data.append(buffer.data(), bytes);
    }

    auto headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos) throw std::runtime_error("incomplete HTTP headers");

    std::size_t headerSize = headerEnd + 4;
    std::string headerPart = data.substr(0, headerSize);

    std::size_t contentLength = 0;
    if (!parseHeaderContentLength(headerPart, contentLength)) return headerPart;
    if (contentLength == 0) return headerPart;
    if (contentLength > config.maxBodyBytes) return headerPart;

    std::size_t alreadyHave = data.size() > headerSize ? data.size() - headerSize : 0;
    if (alreadyHave >= contentLength) {
        data.resize(headerSize + contentLength);
        return data;
    }

    while (alreadyHave < contentLength) {
        boost::system::error_code ec;
        std::size_t remaining = contentLength - alreadyHave;
        std::size_t chunkSize = std::min(buffer.size(), remaining);
        std::size_t bytes = stream.read_some(boost::asio::buffer(buffer.data(), chunkSize), ec);
        if (ec == boost::asio::error::eof) break;
        if (ec) throw std::runtime_error("failed to read request body: " + ec.message());
        data.append(buffer.data(), bytes);
        alreadyHave += bytes;
    }

    return data;
}


struct HeaderReadResult {
    std::string headerPart;
    std::string leftover;
};

template <typename SyncReadStream>
HeaderReadResult readHeadersOnly(SyncReadStream& stream, const ServerConfig& config) {
    std::string data;
    data.reserve(std::min<std::size_t>(config.maxRequestBytes, 4096));
    std::array<char, 512> buffer{};

    while (data.find("\r\n\r\n") == std::string::npos) {
        if (data.size() >= config.maxRequestBytes) throw std::runtime_error("request headers too large");
        boost::system::error_code ec;
        std::size_t remaining = config.maxRequestBytes - data.size();
        std::size_t chunkSize = std::min(buffer.size(), remaining);
        std::size_t bytes = stream.read_some(boost::asio::buffer(buffer.data(), chunkSize), ec);
        if (ec == boost::asio::error::eof) break;
        if (ec) throw std::runtime_error("failed to read request headers: " + ec.message());
        data.append(buffer.data(), bytes);
    }

    auto headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos) throw std::runtime_error("incomplete HTTP headers");

    HeaderReadResult result;
    result.headerPart = data.substr(0, headerEnd + 4);
    result.leftover = data.substr(headerEnd + 4);
    return result;
}

bool localIsToken(const std::string& value) {
    if (value.empty()) return false;
    const std::string allowed = "!#$%&'*+-.^_`|~";
    for (unsigned char c : value) {
        if (!(std::isalnum(c) || allowed.find(static_cast<char>(c)) != std::string::npos)) return false;
    }
    return true;
}

HttpParseResult parseHeadersOnly(const std::string& headerPart, const HttpParseLimits& limits) {
    auto fail = [](int status, std::string reason, std::string body) {
        HttpParseResult r;
        r.ok = false;
        r.status = status;
        r.reason = std::move(reason);
        r.message = std::move(body);
        return r;
    };

    HttpParseResult result;

    auto headerEnd = headerPart.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return fail(400, "Bad Request", "400 Bad Request\n");

    auto lineEnd = headerPart.find("\r\n");
    if (lineEnd == std::string::npos || lineEnd > headerEnd) return fail(400, "Bad Request", "400 Bad Request\n");
    if (lineEnd > limits.maxHeaderLineBytes) return fail(431, "Request Header Fields Too Large", "431 Request Header Fields Too Large\n");

    std::istringstream first(headerPart.substr(0, lineEnd));
    std::string extra;
    first >> result.request.method >> result.request.target >> result.request.version;
    if (first >> extra) return fail(400, "Bad Request", "400 Bad Request\n");

    if (result.request.method.empty() || result.request.target.empty() || result.request.version.empty()) return fail(400, "Bad Request", "400 Bad Request\n");
    if (result.request.method.size() > limits.maxMethodBytes) return fail(405, "Method Not Allowed", "405 Method Not Allowed\n");
    if (!localIsToken(result.request.method)) return fail(400, "Bad Request", "400 Bad Request\n");
    if (result.request.target.size() > limits.maxUriBytes) return fail(414, "URI Too Long", "414 URI Too Long\n");
    if (result.request.version != "HTTP/1.1" && result.request.version != "HTTP/1.0") return fail(505, "HTTP Version Not Supported", "505 HTTP Version Not Supported\n");

    std::size_t start = lineEnd + 2;
    std::size_t headerCount = 0;
    bool sawHost = false;
    std::string contentLengthText;

    while (true) {
        auto end = headerPart.find("\r\n", start);
        if (end == std::string::npos) return fail(400, "Bad Request", "400 Bad Request\n");
        if (end == start) break;
        if (end > headerEnd) return fail(400, "Bad Request", "400 Bad Request\n");
        if (end - start > limits.maxHeaderLineBytes) return fail(431, "Request Header Fields Too Large", "431 Request Header Fields Too Large\n");
        if (++headerCount > limits.maxHeaders) return fail(431, "Request Header Fields Too Large", "431 Request Header Fields Too Large\n");

        std::string line = headerPart.substr(start, end - start);
        if (!line.empty() && (line.front() == ' ' || line.front() == '\t')) return fail(400, "Bad Request", "400 Bad Request\n");

        auto colon = line.find(':');
        if (colon == std::string::npos) return fail(400, "Bad Request", "400 Bad Request\n");

        std::string name = lowercaseLocal(line.substr(0, colon));
        std::string value = trimHeaderValue(line.substr(colon + 1));

        if (!localIsToken(name)) return fail(400, "Bad Request", "400 Bad Request\n");

        if (name == "host") {
            if (sawHost) return fail(400, "Bad Request", "400 Bad Request\n");
            sawHost = true;
        }

        if (name == "transfer-encoding") return fail(501, "Not Implemented", "501 Not Implemented\n");

        if (name == "content-length") {
            if (!contentLengthText.empty() && contentLengthText != value) return fail(400, "Bad Request", "400 Bad Request\n");
            contentLengthText = value;
        }

        result.request.headers[name] = value;
        start = end + 2;
    }

    if (result.request.version == "HTTP/1.1" && !sawHost) return fail(400, "Bad Request", "400 Bad Request\n");

    if (!contentLengthText.empty()) {
        for (unsigned char c : contentLengthText) {
            if (!std::isdigit(c)) return fail(400, "Bad Request", "400 Bad Request\n");
        }
        try {
            result.request.hasContentLength = true;
            result.request.contentLength = static_cast<std::size_t>(std::stoull(contentLengthText));
            if (result.request.contentLength > limits.maxBodyBytes) return fail(413, "Payload Too Large", "413 Payload Too Large\n");
        } catch (...) {
            return fail(400, "Bad Request", "400 Bad Request\n");
        }
    }

    result.ok = true;
    result.status = 200;
    result.reason = "OK";
    return result;
}

template <typename SyncReadStream>
std::string completeRequestInMemory(SyncReadStream& stream, const ServerConfig& config, const HeaderReadResult& headers) {
    std::string data = headers.headerPart + headers.leftover;
    std::size_t contentLength = 0;

    if (!parseHeaderContentLength(headers.headerPart, contentLength) || contentLength == 0) {
        return headers.headerPart;
    }

    if (contentLength > config.maxBodyBytes) return headers.headerPart;

    std::size_t alreadyHave = headers.leftover.size();
    std::array<char, 512> buffer{};

    while (alreadyHave < contentLength) {
        boost::system::error_code ec;
        std::size_t remaining = contentLength - alreadyHave;
        std::size_t chunkSize = std::min(buffer.size(), remaining);
        std::size_t bytes = stream.read_some(boost::asio::buffer(buffer.data(), chunkSize), ec);
        if (ec == boost::asio::error::eof) break;
        if (ec) throw std::runtime_error("failed to read request body: " + ec.message());
        data.append(buffer.data(), bytes);
        alreadyHave += bytes;
    }

    if (data.size() > headers.headerPart.size() + contentLength) {
        data.resize(headers.headerPart.size() + contentLength);
    }

    return data;
}

std::filesystem::path uniqueSpoolPath(const std::filesystem::path& directory) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto count = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    return directory / ("upload_spool_" + std::to_string(count) + ".tmp");
}

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

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void configureAcceptedSocket(tcp::socket& socket, const ServerConfig& config) {
    boost::system::error_code ignored;
    socket.set_option(tcp::no_delay(config.tcpNoDelay), ignored);
    socket.set_option(boost::asio::socket_base::keep_alive(config.tcpKeepAlive), ignored);
    if (config.socketReceiveBufferBytes > 0) {
        socket.set_option(boost::asio::socket_base::receive_buffer_size(config.socketReceiveBufferBytes), ignored);
    }
    if (config.socketSendBufferBytes > 0) {
        socket.set_option(boost::asio::socket_base::send_buffer_size(config.socketSendBufferBytes), ignored);
    }
}

template <typename SyncWriteStream>
std::size_t writeHttpResponse(SyncWriteStream& stream, const HttpResponse& response) {
    const std::string headers = response.headersToString();
    if (response.headOnly || response.bodySize() == 0) {
        boost::asio::write(stream, boost::asio::buffer(headers));
        return headers.size();
    }

    const auto& body = response.bodyData();
    const std::array<boost::asio::const_buffer, 2> buffers{
        boost::asio::buffer(headers),
        boost::asio::buffer(body)
    };
    boost::asio::write(stream, buffers);
    return headers.size() + body.size();
}
}

Server::Server(boost::asio::io_context& io, ServerConfig config)
    : io_(io), plainAcceptor_(io),
      config_(std::move(config)),
      authStore_(config_.authTokens, config_.authSecretsFile, config_.authHotReload, config_.authReloadIntervalSeconds),
      logger_(config_.accessLog, config_.errorLog, config_.auditLog,
              config_.auditRotateBytes, config_.auditRotateKeep,
              config_.asyncAccessLog, config_.enableAccessLog,
              config_.accessLogSampleRate, config_.accessLogQueueCapacity,
              config_.accessLogFlushIntervalMs),
      rateLimiter_(config_.rateLimitPerMinute, config_.rateLimiterShards),
      workers_(config_.workerThreads, config_.maxPendingConnections),
      staticCache_(config_.enableStaticCache, config_.staticCacheShards,
                   config_.staticCacheMaxEntries, config_.staticCacheMaxBytes,
                   config_.staticCacheMaxFileBytes, config_.staticCacheRevalidateMs),
      sqlite_(config_.sqliteDatabase),
      startedAt_(std::chrono::steady_clock::now()) {
    config_.defaultRoot = fs::weakly_canonical(config_.defaultRoot);
    for (auto& [host, root] : config_.virtualHosts) root = fs::weakly_canonical(root);

    plainAcceptor_.open(tcp::v4());
    plainAcceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    plainAcceptor_.bind(tcp::endpoint(tcp::v4(), config_.port));
    plainAcceptor_.listen(config_.listenBacklog);

#if defined(HYDROGENHTTPD_ENABLE_TLS)
    if (config_.enableTls) {
        configureTlsContext();
        tlsAcceptor_ = std::make_unique<tcp::acceptor>(io_);
        tlsAcceptor_->open(tcp::v4());
        tlsAcceptor_->set_option(boost::asio::socket_base::reuse_address(true));
        tlsAcceptor_->bind(tcp::endpoint(tcp::v4(), config_.tlsPort));
        tlsAcceptor_->listen(config_.listenBacklog);
    }
#endif
}

void Server::run() {
    std::cout << "HydrogenHttpd v" << HYDROGENHTTPD_VERSION << " server running\n";
    std::cout << "HTTP  port: " << plainAcceptor_.local_endpoint().port() << "\n";
    std::cout << "I/O threads: " << config_.ioThreads << "\n";
    std::cout << "Worker threads: " << config_.workerThreads << "\n";
    std::cout << "Listen backlog: " << config_.listenBacklog << "\n";
    std::cout << "Static cache: " << (config_.enableStaticCache ? "enabled" : "disabled") << "\n";
    std::cout << ".htaccess: " << (config_.enableHtaccess ? "enabled" : "disabled") << "\n";
    std::cout << "Force HTTPS: " << (config_.forceHttps ? "enabled" : "disabled") << "\n";
#if defined(HYDROGENHTTPD_ENABLE_TLS)
    if (config_.enableTls && tlsAcceptor_) std::cout << "HTTPS port: " << tlsAcceptor_->local_endpoint().port() << "\n";
    else std::cout << "HTTPS: disabled\n";
#endif
    if (config_.enableSql) {
        if (!sqlite_.initialize()) logger_.error("sql_module initialization_failed");
        else logger_.error("sql_module initialized");
    }
    acceptPlain();
#if defined(HYDROGENHTTPD_ENABLE_TLS)
    if (config_.enableTls && tlsAcceptor_) acceptTls();
#endif
}

void Server::acceptPlain() {
    plainAcceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            configureAcceptedSocket(socket, config_);
            auto socketPtr = std::make_shared<tcp::socket>(std::move(socket));
            bool accepted = workers_.enqueue([this, socketPtr]() mutable { handlePlainClient(std::move(*socketPtr)); });
            if (!accepted) { logger_.error("worker_queue_full scheme=http"); boost::system::error_code ignored; socketPtr->close(ignored); }
        }
        acceptPlain();
    });
}

#if defined(HYDROGENHTTPD_ENABLE_TLS)
void Server::configureTlsContext() {
    namespace ssl = boost::asio::ssl;
    tlsContext_ = std::make_unique<ssl::context>(ssl::context::tls_server);
    tlsContext_->set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::no_sslv3 | ssl::context::no_tlsv1 | ssl::context::no_tlsv1_1 | ssl::context::single_dh_use);
    tlsContext_->use_certificate_chain_file(config_.tlsCertFile.string());
    tlsContext_->use_private_key_file(config_.tlsKeyFile.string(), ssl::context::pem);
}

void Server::acceptTls() {
    tlsAcceptor_->async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            configureAcceptedSocket(socket, config_);
            auto socketPtr = std::make_shared<tcp::socket>(std::move(socket));
            bool accepted = workers_.enqueue([this, socketPtr]() mutable { handleTlsClient(std::move(*socketPtr)); });
            if (!accepted) { logger_.error("worker_queue_full scheme=https"); boost::system::error_code ignored; socketPtr->close(ignored); }
        }
        acceptTls();
    });
}
#endif

void Server::handlePlainClient(tcp::socket socket) {
    std::string clientIp = "unknown";
    try {
        clientIp = socket.remote_endpoint().address().to_string();
        if (config_.enableRateLimiter && !rateLimiter_.allow("http:" + clientIp)) {
            HttpResponse res; res.exposeServerHeader = config_.exposeServerHeader; res.status = 429; res.reason = "Too Many Requests"; res.body = "429 Too Many Requests\n";
            const auto bytes = writeHttpResponse(socket, res);
            logger_.access(clientIp, "http", "-", "-", 429, bytes); return;
        }
        setReceiveTimeout(socket.native_handle(), config_.readTimeoutSeconds);
        int status = 500; std::string method = "-", target = "-";
        auto response = handleClientStream(socket, false, clientIp, status, method, target);
        const auto bytes = writeHttpResponse(socket, response);
        logger_.access(clientIp, "http", method, target, status, bytes);
        boost::system::error_code ignored; socket.shutdown(tcp::socket::shutdown_both, ignored); socket.close(ignored);
    } catch (const std::exception& e) {
        logger_.error("scheme=http client=" + clientIp + " error=\"" + e.what() + "\"");
        boost::system::error_code ignored; socket.close(ignored);
    }
}

#if defined(HYDROGENHTTPD_ENABLE_TLS)
void Server::handleTlsClient(tcp::socket socket) {
    std::string clientIp = "unknown";
    try {
        clientIp = socket.remote_endpoint().address().to_string();
        setReceiveTimeout(socket.native_handle(), config_.readTimeoutSeconds);
        boost::asio::ssl::stream<tcp::socket> stream(std::move(socket), *tlsContext_);
        stream.handshake(boost::asio::ssl::stream_base::server);

        if (config_.enableRateLimiter && !rateLimiter_.allow("https:" + clientIp)) {
            HttpResponse res; res.exposeServerHeader = config_.exposeServerHeader; res.tls = true; res.status = 429; res.reason = "Too Many Requests"; res.body = "429 Too Many Requests\n";
            const auto bytes = writeHttpResponse(stream, res);
            logger_.access(clientIp, "https", "-", "-", 429, bytes); return;
        }

        int status = 500; std::string method = "-", target = "-";
        auto response = handleClientStream(stream, true, clientIp, status, method, target);
        const auto bytes = writeHttpResponse(stream, response);
        logger_.access(clientIp, "https", method, target, status, bytes);
        boost::system::error_code ignored; stream.shutdown(ignored); stream.lowest_layer().close(ignored);
    } catch (const std::exception& e) {
        logger_.error("scheme=https client=" + clientIp + " error=\"" + e.what() + "\"");
    }
}
#endif


template <typename SyncReadStream>
HttpResponse Server::handleClientStream(SyncReadStream& stream, bool tls, const std::string& clientIp, int& statusOut, std::string& methodOut, std::string& targetOut) {
    auto headers = readHeadersOnly(stream, config_);

    HttpParseLimits limits{
        config_.maxHeaderLineBytes,
        config_.maxHeaders,
        config_.maxUriBytes,
        config_.maxMethodBytes,
        config_.maxBodyBytes
    };

    auto parsed = parseHeadersOnly(headers.headerPart, limits);
    if (!parsed.ok) {
        HttpResponse res;
        res.tls = tls;
        res.exposeServerHeader = config_.exposeServerHeader;
        res.status = parsed.status;
        res.reason = parsed.reason;
        res.body = parsed.message;
        statusOut = res.status;
        return res;
    }

    HttpRequest req = std::move(parsed.request);
    methodOut = req.method.empty() ? "-" : req.method;
    targetOut = req.target.empty() ? "-" : req.target;

    if (req.target == config_.adminStatusEndpoint) {
        HttpResponse res;
        res.tls = tls;
        res.exposeServerHeader = config_.exposeServerHeader;

        if (!config_.enableAdminStatus) {
            res.status = 404;
            res.reason = "Not Found";
            res.body = "404 Not Found\n";
            statusOut = res.status;
            return res;
        }

        if (req.method != "GET") {
            res.status = 405;
            res.reason = "Method Not Allowed";
            res.body = "405 Method Not Allowed\n";
            statusOut = res.status;
            return res;
        }

        const auto decision = authorizeEndpoint(req, "admin");
        if (!decision.allowed()) {
            if (decision.code == AuthDecisionCode::Missing) {
                auditEndpointEvent("auth_missing", clientIp, req.target, "admin", "-", 401);
                return unauthorizedResponse(tls, statusOut);
            }
            if (decision.code == AuthDecisionCode::InsufficientScope) {
                auditEndpointEvent("auth_scope_denied", clientIp, req.target, "admin", decision.tokenName, 403);
                return forbiddenResponse(tls, statusOut);
            }
            if (decision.code == AuthDecisionCode::Expired) {
                auditEndpointEvent("auth_expired", clientIp, req.target, "admin", decision.tokenName, 401);
                return unauthorizedResponse(tls, statusOut, true);
            }

            auditEndpointEvent("auth_invalid", clientIp, req.target, "admin", "unknown", 401);
            return unauthorizedResponse(tls, statusOut, true);
        }

        auditEndpointEvent("auth_allowed", clientIp, req.target, "admin", decision.tokenName, 200);
        return adminStatusResponse(tls, statusOut);
    }

    if (req.target == config_.uploadEndpoint) {
        HttpResponse res;
        res.tls = tls;
        res.exposeServerHeader = config_.exposeServerHeader;

        if (!config_.enableUploads) {
            res.status = 404;
            res.reason = "Not Found";
            res.body = "404 Not Found\n";
            statusOut = res.status;
            return res;
        }

        if (req.method != "POST") {
            res.status = 405;
            res.reason = "Method Not Allowed";
            res.body = "405 Method Not Allowed\n";
            statusOut = res.status;
            return res;
        }

        const auto decision = authorizeEndpoint(req, "upload");
        if (!decision.allowed()) {
            if (decision.code == AuthDecisionCode::Missing) {
                auditEndpointEvent("auth_missing", clientIp, req.target, "upload", "-", 401);
                return unauthorizedResponse(tls, statusOut);
            }
            if (decision.code == AuthDecisionCode::InsufficientScope) {
                auditEndpointEvent("auth_scope_denied", clientIp, req.target, "upload", decision.tokenName, 403);
                return forbiddenResponse(tls, statusOut);
            }
            if (decision.code == AuthDecisionCode::Expired) {
                auditEndpointEvent("auth_expired", clientIp, req.target, "upload", decision.tokenName, 401);
                return unauthorizedResponse(tls, statusOut, true);
            }

            auditEndpointEvent("auth_invalid", clientIp, req.target, "upload", "unknown", 401);
            return unauthorizedResponse(tls, statusOut, true);
        }

        auditEndpointEvent("auth_allowed", clientIp, req.target, "upload", decision.tokenName, 200);

        auto ct = req.headers.find("content-type");
        if (ct == req.headers.end()) {
            res.status = 415;
            res.reason = "Unsupported Media Type";
            res.body = "415 Unsupported Media Type\n";
            statusOut = res.status;
            return res;
        }

        if (!req.hasContentLength || req.contentLength == 0) {
            res.status = 400;
            res.reason = "Bad Request";
            res.body = "400 Bad Request\n";
            statusOut = res.status;
            return res;
        }

        if (req.contentLength > config_.maxBodyBytes) {
            res.status = 413;
            res.reason = "Payload Too Large";
            res.body = "413 Payload Too Large\n";
            statusOut = res.status;
            return res;
        }

        std::filesystem::create_directories(config_.uploadSpoolDirectory);
        auto spoolPath = uniqueSpoolPath(config_.uploadSpoolDirectory);

        try {
            std::ofstream spool(spoolPath, std::ios::binary | std::ios::trunc);
            if (!spool) throw std::runtime_error("failed to create upload spool");

            std::size_t written = 0;
            if (!headers.leftover.empty()) {
                std::size_t n = std::min(headers.leftover.size(), req.contentLength);
                spool.write(headers.leftover.data(), static_cast<std::streamsize>(n));
                written += n;
            }

            std::array<char, 8192> buffer{};
            while (written < req.contentLength) {
                boost::system::error_code ec;
                std::size_t remaining = req.contentLength - written;
                std::size_t chunk = std::min(buffer.size(), remaining);
                std::size_t bytes = stream.read_some(boost::asio::buffer(buffer.data(), chunk), ec);
                if (ec == boost::asio::error::eof) break;
                if (ec) throw std::runtime_error("failed to stream upload body: " + ec.message());
                spool.write(buffer.data(), static_cast<std::streamsize>(bytes));
                written += bytes;
            }

            spool.close();

            if (written != req.contentLength) {
                std::filesystem::remove(spoolPath);
                res.status = 400;
                res.reason = "Bad Request";
                res.body = "400 Bad Request\n";
                statusOut = res.status;
                return res;
            }

            MultipartLimits uploadLimits{
                config_.maxMultipartParts,
                config_.maxUploadFileBytes,
                config_.maxUploadFieldBytes
            };

            auto result = Multipart::saveUploadsFromSpoolFile(spoolPath, ct->second, config_.uploadDirectory, uploadLimits);
            std::filesystem::remove(spoolPath);

            if (!result.ok) {
                res.status = result.status;
                if (res.status == 413) res.reason = "Payload Too Large";
                else if (res.status == 415) res.reason = "Unsupported Media Type";
                else if (res.status == 500) res.reason = "Internal Server Error";
                else res.reason = "Bad Request";
                res.body = std::to_string(res.status) + " " + res.reason + "\n";
                statusOut = res.status;
                return res;
            }

            std::ostringstream json;
            json << "{";
            json << "\"ok\":true,";
            json << "\"streamed\":true,";
            json << "\"files\":[";
            for (std::size_t i = 0; i < result.files.size(); ++i) {
                const auto& f = result.files[i];
                if (i > 0) json << ",";
                json << "{";
                json << "\"field\":\"" << f.fieldName << "\",";
                json << "\"original\":\"" << f.originalFilename << "\",";
                json << "\"stored\":\"" << f.storedFilename << "\",";
                json << "\"size\":" << f.size;
                json << "}";
            }
            json << "]}";

            res.status = 200;
            res.reason = "OK";
            res.contentType = "application/json; charset=utf-8";
            res.body = json.str();
            statusOut = res.status;
            return res;
        } catch (...) {
            std::filesystem::remove(spoolPath);
            throw;
        }
    }

    auto rawRequest = completeRequestInMemory(stream, config_, headers);
    return makeResponse(rawRequest, tls, statusOut, methodOut, targetOut);
}


std::string Server::readPlainRequest(tcp::socket& socket) { return readRequestWithBody(socket, config_); }
#if defined(HYDROGENHTTPD_ENABLE_TLS)
std::string Server::readTlsRequest(boost::asio::ssl::stream<tcp::socket>& stream) { return readRequestWithBody(stream, config_); }
#endif

HttpResponse Server::makeResponse(const std::string& rawRequest, bool tls, int& statusOut, std::string& methodOut, std::string& targetOut) {
    HttpResponse res; res.tls = tls; res.exposeServerHeader = config_.exposeServerHeader;
    auto finish = [&]() { statusOut = res.status; return res; };

    if (rawRequest.empty() || rawRequest.size() > config_.maxRequestBytes) { res.status = 400; res.reason = "Bad Request"; res.body = "400 Bad Request\n"; return finish(); }

    HttpParseLimits limits{config_.maxHeaderLineBytes, config_.maxHeaders, config_.maxUriBytes, config_.maxMethodBytes, config_.maxBodyBytes};
    auto parsed = StrictHttpParser::parse(rawRequest, limits);
    if (!parsed.ok) { res.status = parsed.status; res.reason = parsed.reason; res.body = parsed.message; return finish(); }

    HttpRequest req = std::move(parsed.request);
    methodOut = req.method.empty() ? "-" : req.method;
    targetOut = req.target.empty() ? "-" : req.target;

    if (!security::isMethodAllowed(req.method)) { res.status = 405; res.reason = "Method Not Allowed"; res.body = "405 Method Not Allowed\n"; return finish(); }

    if (req.contentLength > 0 && req.method != "POST") {
        res.status = 413;
        res.reason = "Payload Too Large";
        res.body = "413 Payload Too Large\n";
        return finish();
    }

    std::string host; auto hostIt = req.headers.find("host"); if (hostIt != req.headers.end()) host = hostIt->second;

    if (!tls && config_.forceHttps) {
        res.status = config_.httpsRedirectStatus;
        res.reason = config_.httpsRedirectStatus == 301 ? "Moved Permanently" : "Permanent Redirect";
        res.body = res.reason + "\n";
        res.extraHeaders["Location"] = httpsRedirectLocation(host, req.target);
        return finish();
    }

    if (req.target == config_.uploadEndpoint) {
        return handleUploadRequest(req, res, statusOut);
    }

    res.headOnly = req.method == "HEAD";
    auto documentRoot = selectDocumentRoot(host);
    auto safePath = security::resolveSafePath(documentRoot, req.target);
    if (!safePath.has_value()) { res.status = 403; res.reason = "Forbidden"; res.body = "403 Forbidden\n"; return finish(); }

    if (isForbiddenFile(documentRoot, *safePath)) { res.status = 403; res.reason = "Forbidden"; res.body = "403 Forbidden\n"; return finish(); }

    HtaccessRules rules;
    if (config_.enableHtaccess) {
        rules = Htaccess::evaluate(documentRoot, *safePath, config_.htaccessFilename, config_.maxHtaccessDepth);
        for (const auto& [k, v] : rules.responseHeaders) res.extraHeaders[k] = v;
        if (rules.denyAll) { res.status = 403; res.reason = "Forbidden"; res.body = "403 Forbidden\n"; return finish(); }
    }

    if (!fs::exists(*safePath) || !fs::is_regular_file(*safePath)) { res.status = 404; res.reason = "Not Found"; res.body = "404 Not Found\n"; return finish(); }

    if (safePath->extension() == config_.phpExtension) {
        if (!config_.enablePhp) {
            res.status = 403;
            res.reason = "Forbidden";
            res.body = "403 Forbidden\n";
            return finish();
        }

        int phpStatus = 500;
        std::string contentType;
        auto ct = req.headers.find("content-type");
        if (ct != req.headers.end()) contentType = ct->second;

        res.body = handlePhpScript(*safePath, req.target, req.method, tls, req.body, contentType, phpStatus);
        res.status = phpStatus;
        res.reason = phpStatus == 200 ? "OK" : "PHP FastCGI Error";
        res.contentType = "text/html; charset=utf-8";
        return finish();
    }

    if (req.method == "POST") {
        res.status = 405;
        res.reason = "Method Not Allowed";
        res.body = "405 Method Not Allowed\n";
        return finish();
    }

    res.contentType = mimeType(*safePath);
    res.cacheControl = config_.staticCacheControl;

    auto cached = staticCache_.get(*safePath);
    if (!cached || !cached->body) {
        res.status = 500;
        res.reason = "Internal Server Error";
        res.cacheControl = "no-store";
        res.body = "500 Internal Server Error\n";
        return finish();
    }

    res.extraHeaders["ETag"] = cached->etag;
    auto inm = req.headers.find("if-none-match");
    if (inm != req.headers.end() && inm->second == cached->etag) {
        res.status = 304;
        res.reason = "Not Modified";
        res.body.clear();
        res.sharedBody.reset();
        return finish();
    }

    res.sharedBody = cached->body;
    return finish();
}


HttpResponse Server::handleUploadRequest(const HttpRequest& req, HttpResponse res, int& statusOut) {
    auto finish = [&]() { statusOut = res.status; return res; };

    if (!config_.enableUploads) {
        res.status = 404;
        res.reason = "Not Found";
        res.body = "404 Not Found\n";
        return finish();
    }

    if (req.method != "POST") {
        res.status = 405;
        res.reason = "Method Not Allowed";
        res.body = "405 Method Not Allowed\n";
        return finish();
    }

    const auto decision = authorizeEndpoint(req, "upload");
    if (!decision.allowed()) {
        if (decision.code == AuthDecisionCode::InsufficientScope) return forbiddenResponse(res.tls, statusOut);
        return unauthorizedResponse(res.tls, statusOut, decision.code != AuthDecisionCode::Missing);
    }

    auto ct = req.headers.find("content-type");
    if (ct == req.headers.end()) {
        res.status = 415;
        res.reason = "Unsupported Media Type";
        res.body = "415 Unsupported Media Type\n";
        return finish();
    }

    MultipartLimits limits{
        config_.maxMultipartParts,
        config_.maxUploadFileBytes,
        config_.maxUploadFieldBytes
    };

    auto result = Multipart::saveUploads(req.body, ct->second, config_.uploadDirectory, limits);
    if (!result.ok) {
        res.status = result.status;
        if (res.status == 413) res.reason = "Payload Too Large";
        else if (res.status == 415) res.reason = "Unsupported Media Type";
        else if (res.status == 500) res.reason = "Internal Server Error";
        else res.reason = "Bad Request";
        res.body = std::to_string(res.status) + " " + res.reason + "\n";
        return finish();
    }

    std::ostringstream json;
    json << "{";
    json << "\"ok\":true,";
    json << "\"files\":[";
    for (std::size_t i = 0; i < result.files.size(); ++i) {
        const auto& f = result.files[i];
        if (i > 0) json << ",";
        json << "{";
        json << "\"field\":\"" << f.fieldName << "\",";
        json << "\"original\":\"" << f.originalFilename << "\",";
        json << "\"stored\":\"" << f.storedFilename << "\",";
        json << "\"size\":" << f.size;
        json << "}";
    }
    json << "]}";

    res.status = 200;
    res.reason = "OK";
    res.contentType = "application/json; charset=utf-8";
    res.body = json.str();
    return finish();
}



AuthDecision Server::authorizeEndpoint(const HttpRequest& req, const std::string& requiredScope) {
    if (!config_.enableEndpointAuth) return {AuthDecisionCode::Allowed, "auth-disabled"};

    auto it = req.headers.find("authorization");
    const std::string header = it == req.headers.end() ? std::string{} : it->second;
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    AuthReloadResult reload;
    auto decision = authStore_.authorize(
        header,
        config_.authBearerToken,
        requiredScope,
        static_cast<std::int64_t>(now),
        &reload
    );

    if (reload.code == AuthReloadCode::Reloaded) {
        logger_.audit(
            "event=auth_store_reloaded"
            " generation=" + std::to_string(reload.generation) +
            " token_count=" + std::to_string(reload.tokenCount)
        );
    } else if (reload.code == AuthReloadCode::Failed) {
        logger_.error("auth_store_reload_failed error=\"" + reload.message + "\"");
        logger_.audit(
            "event=auth_store_reload_failed"
            " generation=" + std::to_string(reload.generation) +
            " token_count=" + std::to_string(reload.tokenCount)
        );
    }

    return decision;
}

HttpResponse Server::unauthorizedResponse(bool tls, int& statusOut, bool invalidToken) const {
    HttpResponse res;
    res.tls = tls;
    res.exposeServerHeader = config_.exposeServerHeader;
    res.status = 401;
    res.reason = "Unauthorized";
    res.body = "401 Unauthorized\n";
    res.extraHeaders["WWW-Authenticate"] = invalidToken ? "Bearer error=\"invalid_token\"" : "Bearer";
    statusOut = res.status;
    return res;
}

HttpResponse Server::forbiddenResponse(bool tls, int& statusOut) const {
    HttpResponse res;
    res.tls = tls;
    res.exposeServerHeader = config_.exposeServerHeader;
    res.status = 403;
    res.reason = "Forbidden";
    res.body = "403 Forbidden\n";
    statusOut = res.status;
    return res;
}

HttpResponse Server::adminStatusResponse(bool tls, int& statusOut) const {
    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - startedAt_
    ).count();

    HttpResponse res;
    res.tls = tls;
    res.exposeServerHeader = config_.exposeServerHeader;
    res.status = 200;
    res.reason = "OK";
    res.contentType = "application/json; charset=utf-8";

    std::ostringstream json;
    json << "{";
    json << "\"service\":\"HydrogenHttpd\",";
    json << "\"version\":\"" << HYDROGENHTTPD_VERSION << "\",";
    json << "\"uptime_seconds\":" << uptime << ",";
    json << "\"worker_threads\":" << config_.workerThreads << ",";
    json << "\"queued_tasks\":" << workers_.queued() << ",";
    json << "\"active_workers\":" << workers_.active() << ",";
    json << "\"completed_tasks\":" << workers_.completed() << ",";
    json << "\"rejected_tasks\":" << workers_.rejected() << ",";
    json << "\"max_pending_connections\":" << config_.maxPendingConnections << ",";
    json << "\"rate_limiter_enabled\":" << (config_.enableRateLimiter ? "true" : "false") << ",";
    json << "\"rate_limiter_buckets\":" << rateLimiter_.bucketCount() << ",";
    json << "\"dropped_access_logs\":" << logger_.droppedAccessLogs() << ",";
    json << "\"static_cache_entries\":" << staticCache_.entries() << ",";
    json << "\"static_cache_bytes\":" << staticCache_.bytes() << ",";
    json << "\"static_cache_hits\":" << staticCache_.hits() << ",";
    json << "\"static_cache_misses\":" << staticCache_.misses() << ",";
    json << "\"static_cache_evictions\":" << staticCache_.evictions() << ",";
    json << "\"tls_enabled\":" << (config_.enableTls ? "true" : "false") << ",";
    json << "\"uploads_enabled\":" << (config_.enableUploads ? "true" : "false") << ",";
    json << "\"php_enabled\":" << (config_.enablePhp ? "true" : "false") << ",";
    json << "\"endpoint_auth_enabled\":" << (config_.enableEndpointAuth ? "true" : "false") << ",";
    json << "\"configured_token_rules\":" << authStore_.tokenCount() << ",";
    json << "\"auth_store_generation\":" << authStore_.generation() << ",";
    json << "\"auth_last_reload_epoch\":" << authStore_.lastSuccessfulReloadEpoch() << ",";
    json << "\"auth_hot_reload_enabled\":" << (authStore_.hotReloadEnabled() ? "true" : "false");
    json << "}";

    res.body = json.str();
    statusOut = res.status;
    return res;
}

void Server::auditEndpointEvent(
    const std::string& event,
    const std::string& clientIp,
    const std::string& target,
    const std::string& requiredScope,
    const std::string& tokenName,
    int status
) {
    logger_.audit(
        "event=" + event +
        " ip=" + clientIp +
        " target=\"" + target + "\"" +
        " required_scope=" + requiredScope +
        " token=\"" + tokenName + "\"" +
        " status=" + std::to_string(status)
    );
}

fs::path Server::selectDocumentRoot(const std::string& host) const {
    auto normalized = security::normalizeHost(host);
    auto it = config_.virtualHosts.find(normalized);
    if (it != config_.virtualHosts.end()) return it->second;
    return config_.defaultRoot;
}

bool Server::isForbiddenFile(const fs::path& documentRoot, const fs::path& path) const {
    auto rel = fs::weakly_canonical(path).lexically_relative(fs::weakly_canonical(documentRoot));
    if (config_.denyHiddenFiles) {
        for (const auto& part : rel) {
            auto name = part.string();
            if (name.empty()) continue;
            if (name[0] == '.') {
                if (config_.allowDotWellKnown && name == ".well-known") continue;
                return true;
            }
        }
    }

    auto filename = lower(path.filename().string());
    auto ext = lower(path.extension().string());
    if (filename == config_.htaccessFilename) return true;
    if (!config_.blockSensitiveFiles) return false;

    static const std::set<std::string> blockedNames{
        "server.conf", "server.production.conf", "cmakelists.txt", "license"
    };
    static const std::set<std::string> blockedExt{
        ".conf", ".ini", ".db", ".db-wal", ".db-shm", ".key", ".pem", ".crt", ".csr", ".log", ".pid", ".sqlite", ".sqlite3", ".bak", ".old", ".tmp"
    };
    return blockedNames.count(filename) > 0 || blockedExt.count(ext) > 0;
}

std::string Server::httpsRedirectLocation(const std::string& host, const std::string& target) const {
    std::string normalized = security::normalizeHost(host);
    if (normalized.empty()) normalized = "localhost";
    std::string location = "https://" + normalized;
    unsigned short port = config_.publicHttpsPort ? config_.publicHttpsPort : config_.tlsPort;
    if (port != 443) location += ":" + std::to_string(port);
    location += target.empty() ? "/" : target;
    return location;
}

std::string Server::mimeType(const fs::path& path) const {
    auto ext = lower(path.extension().string());
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".wasm") return "application/wasm";
    return "application/octet-stream";
}

std::string Server::readFile(const fs::path& path) const {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::ostringstream ss; ss << file.rdbuf(); return ss.str();
}

std::string Server::handlePhpScript(
    const std::filesystem::path& scriptPath,
    const std::string& requestUri,
    const std::string& method,
    bool,
    const std::string& body,
    const std::string& contentType,
    int& statusOut
) {
    try {
        std::string uri = requestUri;
        std::string query;
        auto q = uri.find('?');
        if (q != std::string::npos) { query = uri.substr(q + 1); uri = uri.substr(0, q); }

        FastCgiClient client(io_, config_.phpFastcgiHost, config_.phpFastcgiPort, config_.phpFastcgiConnectTimeoutSeconds, config_.phpFastcgiReadTimeoutSeconds);
        FastCgiRequest fcgi;
        fcgi.scriptFilename = scriptPath;
        fcgi.requestMethod = method;
        fcgi.requestUri = uri;
        fcgi.queryString = query;
        fcgi.documentRoot = config_.defaultRoot.string();
        fcgi.body = body;
        fcgi.contentType = contentType;

        auto response = client.execute(fcgi);
        statusOut = response.status;
        return response.body;
    } catch (const std::exception& e) {
        logger_.error(std::string("php_fastcgi error=\"") + e.what() + "\"");
        statusOut = 502;
        return "502 Bad Gateway: PHP FastCGI unavailable\n";
    }
}
