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
}

Server::Server(boost::asio::io_context& io, ServerConfig config)
    : io_(io), plainAcceptor_(io, tcp::endpoint(tcp::v4(), config.port)),
      config_(std::move(config)), logger_(config_.accessLog, config_.errorLog, config_.auditLog),
      rateLimiter_(config_.rateLimitPerMinute),
      workers_(config_.workerThreads, config_.maxPendingConnections),
      sqlite_(config_.sqliteDatabase) {
    config_.defaultRoot = fs::weakly_canonical(config_.defaultRoot);
    for (auto& [host, root] : config_.virtualHosts) root = fs::weakly_canonical(root);
#if defined(HYDROGENHTTPD_ENABLE_TLS)
    if (config_.enableTls) {
        configureTlsContext();
        tlsAcceptor_ = std::make_unique<tcp::acceptor>(io_, tcp::endpoint(tcp::v4(), config_.tlsPort));
    }
#endif
}

void Server::run() {
    std::cout << "HydrogenHttpd v1.5.88 scoped auth + audit running\n";
    std::cout << "HTTP  port: " << plainAcceptor_.local_endpoint().port() << "\n";
    std::cout << "Worker threads: " << config_.workerThreads << "\n";
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
        if (!rateLimiter_.allow("http:" + clientIp)) {
            HttpResponse res; res.exposeServerHeader = config_.exposeServerHeader; res.status = 429; res.reason = "Too Many Requests"; res.body = "429 Too Many Requests\n";
            auto response = res.toString(); boost::asio::write(socket, boost::asio::buffer(response));
            logger_.access(clientIp, "http", "-", "-", 429, response.size()); return;
        }
        setReceiveTimeout(socket.native_handle(), config_.readTimeoutSeconds);
        int status = 500; std::string method = "-", target = "-";
        auto response = handleClientStream(socket, false, clientIp, status, method, target);
        boost::asio::write(socket, boost::asio::buffer(response));
        logger_.access(clientIp, "http", method, target, status, response.size());
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

        if (!rateLimiter_.allow("https:" + clientIp)) {
            HttpResponse res; res.exposeServerHeader = config_.exposeServerHeader; res.tls = true; res.status = 429; res.reason = "Too Many Requests"; res.body = "429 Too Many Requests\n";
            auto response = res.toString(); boost::asio::write(stream, boost::asio::buffer(response));
            logger_.access(clientIp, "https", "-", "-", 429, response.size()); return;
        }

        int status = 500; std::string method = "-", target = "-";
        auto response = handleClientStream(stream, true, clientIp, status, method, target);
        boost::asio::write(stream, boost::asio::buffer(response));
        logger_.access(clientIp, "https", method, target, status, response.size());
        boost::system::error_code ignored; stream.shutdown(ignored); stream.lowest_layer().close(ignored);
    } catch (const std::exception& e) {
        logger_.error("scheme=https client=" + clientIp + " error=\"" + e.what() + "\"");
    }
}
#endif


template <typename SyncReadStream>
std::string Server::handleClientStream(SyncReadStream& stream, bool tls, const std::string& clientIp, int& statusOut, std::string& methodOut, std::string& targetOut) {
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
        return res.toString();
    }

    HttpRequest req = std::move(parsed.request);
    methodOut = req.method.empty() ? "-" : req.method;
    targetOut = req.target.empty() ? "-" : req.target;

    if (req.target == config_.uploadEndpoint) {
        HttpResponse res;
        res.tls = tls;
        res.exposeServerHeader = config_.exposeServerHeader;

        if (!config_.enableUploads) {
            res.status = 404;
            res.reason = "Not Found";
            res.body = "404 Not Found\n";
            statusOut = res.status;
            return res.toString();
        }

        if (req.method != "POST") {
            res.status = 405;
            res.reason = "Method Not Allowed";
            res.body = "405 Method Not Allowed\n";
            statusOut = res.status;
            return res.toString();
        }

        std::string tokenName;
        if (!isEndpointAuthorized(req, "upload", tokenName)) {
            auto authHeader = req.headers.find("authorization");
            if (authHeader == req.headers.end()) {
                auditEndpointEvent("auth_missing", clientIp, req.target, "upload", "-", 401);
                return unauthorizedResponse(tls, statusOut);
            }

            auditEndpointEvent("auth_denied", clientIp, req.target, "upload", tokenName.empty() ? "unknown" : tokenName, 403);
            return forbiddenResponse(tls, statusOut);
        }

        auditEndpointEvent("auth_allowed", clientIp, req.target, "upload", tokenName, 200);

        auto ct = req.headers.find("content-type");
        if (ct == req.headers.end()) {
            res.status = 415;
            res.reason = "Unsupported Media Type";
            res.body = "415 Unsupported Media Type\n";
            statusOut = res.status;
            return res.toString();
        }

        if (!req.hasContentLength || req.contentLength == 0) {
            res.status = 400;
            res.reason = "Bad Request";
            res.body = "400 Bad Request\n";
            statusOut = res.status;
            return res.toString();
        }

        if (req.contentLength > config_.maxBodyBytes) {
            res.status = 413;
            res.reason = "Payload Too Large";
            res.body = "413 Payload Too Large\n";
            statusOut = res.status;
            return res.toString();
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
                return res.toString();
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
                return res.toString();
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
            return res.toString();
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

std::string Server::makeResponse(const std::string& rawRequest, bool tls, int& statusOut, std::string& methodOut, std::string& targetOut) {
    HttpResponse res; res.tls = tls; res.exposeServerHeader = config_.exposeServerHeader;
    auto finish = [&]() { statusOut = res.status; return res.toString(); };

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
    res.body = readFile(*safePath);
    return finish();
}


std::string Server::handleUploadRequest(const HttpRequest& req, HttpResponse& res, int& statusOut) {
    auto finish = [&]() { statusOut = res.status; return res.toString(); };

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

    std::string tokenName;
    if (!isEndpointAuthorized(req, "upload", tokenName)) {
        return unauthorizedResponse(res.tls, statusOut);
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



bool Server::hasScope(const AuthTokenRule& token, const std::string& requiredScope) const {
    for (const auto& scope : token.scopes) {
        if (scope == "*" || scope == requiredScope) return true;
    }
    return false;
}

bool Server::isEndpointAuthorized(const HttpRequest& req, const std::string& requiredScope, std::string& tokenName) const {
    tokenName.clear();

    if (!config_.enableEndpointAuth) {
        tokenName = "auth-disabled";
        return true;
    }

    auto it = req.headers.find("authorization");
    if (it == req.headers.end()) return false;

    const std::string prefix = "Bearer ";
    if (it->second.rfind(prefix, 0) != 0) return false;

    std::string presented = it->second.substr(prefix.size());

    for (const auto& token : config_.authTokens) {
        if (presented == token.token) {
            tokenName = token.name;
            return hasScope(token, requiredScope);
        }
    }

    // Backward-compatible legacy token. Treated as broad admin/upload token.
    if (!config_.authBearerToken.empty() && presented == config_.authBearerToken) {
        tokenName = "legacy";
        return true;
    }

    return false;
}

std::string Server::unauthorizedResponse(bool tls, int& statusOut) const {
    HttpResponse res;
    res.tls = tls;
    res.exposeServerHeader = config_.exposeServerHeader;
    res.status = 401;
    res.reason = "Unauthorized";
    res.body = "401 Unauthorized\n";
    res.extraHeaders["WWW-Authenticate"] = "Bearer";
    statusOut = res.status;
    return res.toString();
}

std::string Server::forbiddenResponse(bool tls, int& statusOut) const {
    HttpResponse res;
    res.tls = tls;
    res.exposeServerHeader = config_.exposeServerHeader;
    res.status = 403;
    res.reason = "Forbidden";
    res.body = "403 Forbidden\n";
    statusOut = res.status;
    return res.toString();
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
