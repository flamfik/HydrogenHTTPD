#include "Server.hpp"
#include "Htaccess.hpp"
#include "Http.hpp"
#include "FastCgiClient.hpp"
#include "Security.hpp"
#include <array>
#include <fstream>
#include <iostream>
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
template <typename SyncReadStream>
std::string readHeadersWithLimit(SyncReadStream& stream, std::size_t maxBytes) {
    std::string data;
    data.reserve(std::min<std::size_t>(maxBytes, 4096));
    std::array<char, 512> buffer{};
    while (data.find("\r\n\r\n") == std::string::npos) {
        if (data.size() >= maxBytes) throw std::runtime_error("request headers too large");
        boost::system::error_code ec;
        std::size_t remaining = maxBytes - data.size();
        std::size_t chunkSize = std::min(buffer.size(), remaining);
        std::size_t bytes = stream.read_some(boost::asio::buffer(buffer.data(), chunkSize), ec);
        if (ec == boost::asio::error::eof) break;
        if (ec) throw std::runtime_error("failed to read request headers: " + ec.message());
        data.append(buffer.data(), bytes);
    }
    if (data.find("\r\n\r\n") == std::string::npos) throw std::runtime_error("incomplete HTTP headers");
    return data;
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
}

Server::Server(boost::asio::io_context& io, ServerConfig config)
    : io_(io), plainAcceptor_(io, tcp::endpoint(tcp::v4(), config.port)),
      config_(std::move(config)), logger_(config_.accessLog, config_.errorLog),
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
    std::cout << "HydrogenHttpd v0.7 running\n";
    std::cout << "HTTP  port: " << plainAcceptor_.local_endpoint().port() << "\n";
    std::cout << "Worker threads: " << config_.workerThreads << "\n";
    std::cout << ".htaccess: " << (config_.enableHtaccess ? "enabled" : "disabled") << "\n";
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
            bool accepted = workers_.enqueue([this, socket = std::move(socket)]() mutable { handlePlainClient(std::move(socket)); });
            if (!accepted) { logger_.error("worker_queue_full scheme=http"); boost::system::error_code ignored; socket.close(ignored); }
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
            bool accepted = workers_.enqueue([this, socket = std::move(socket)]() mutable { handleTlsClient(std::move(socket)); });
            if (!accepted) { logger_.error("worker_queue_full scheme=https"); boost::system::error_code ignored; socket.close(ignored); }
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
            HttpResponse res; res.status = 429; res.reason = "Too Many Requests"; res.body = "429 Too Many Requests\n";
            auto response = res.toString(); boost::asio::write(socket, boost::asio::buffer(response));
            logger_.access(clientIp, "http", "-", "-", 429, response.size()); return;
        }
        setReceiveTimeout(socket.native_handle(), config_.readTimeoutSeconds);
        auto rawRequest = readPlainRequest(socket);
        int status = 500; std::string method = "-", target = "-";
        auto response = makeResponse(rawRequest, false, status, method, target);
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
            HttpResponse res; res.tls = true; res.status = 429; res.reason = "Too Many Requests"; res.body = "429 Too Many Requests\n";
            auto response = res.toString(); boost::asio::write(stream, boost::asio::buffer(response));
            logger_.access(clientIp, "https", "-", "-", 429, response.size()); return;
        }

        auto rawRequest = readTlsRequest(stream);
        int status = 500; std::string method = "-", target = "-";
        auto response = makeResponse(rawRequest, true, status, method, target);
        boost::asio::write(stream, boost::asio::buffer(response));
        logger_.access(clientIp, "https", method, target, status, response.size());
        boost::system::error_code ignored; stream.shutdown(ignored); stream.lowest_layer().close(ignored);
    } catch (const std::exception& e) {
        logger_.error("scheme=https client=" + clientIp + " error=\"" + e.what() + "\"");
    }
}
#endif

std::string Server::readPlainRequest(tcp::socket& socket) { return readHeadersWithLimit(socket, config_.maxRequestBytes); }
#if defined(HYDROGENHTTPD_ENABLE_TLS)
std::string Server::readTlsRequest(boost::asio::ssl::stream<tcp::socket>& stream) { return readHeadersWithLimit(stream, config_.maxRequestBytes); }
#endif

static HttpRequest parseRequest(const std::string& raw) {
    std::istringstream stream(raw); HttpRequest req; stream >> req.method >> req.target >> req.version;
    std::string line; std::getline(stream, line);
    while (std::getline(stream, line)) {
        if (line == "\r" || line.empty()) break;
        auto colon = line.find(':'); if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon), value = line.substr(colon + 1);
        if (!value.empty() && value.back() == '\r') value.pop_back();
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());
        req.headers[key] = value;
    }
    return req;
}

std::string Server::makeResponse(const std::string& rawRequest, bool tls, int& statusOut, std::string& methodOut, std::string& targetOut) {
    HttpResponse res; res.tls = tls;
    auto finish = [&]() { statusOut = res.status; return res.toString(); };

    if (rawRequest.empty() || rawRequest.size() > config_.maxRequestBytes) { res.status = 400; res.reason = "Bad Request"; res.body = "400 Bad Request\n"; return finish(); }
    HttpRequest req = parseRequest(rawRequest);
    methodOut = req.method.empty() ? "-" : req.method; targetOut = req.target.empty() ? "-" : req.target;

    if (req.method.empty() || req.target.empty() || req.version.empty()) { res.status = 400; res.reason = "Bad Request"; res.body = "400 Bad Request\n"; return finish(); }
    if (req.version != "HTTP/1.1" && req.version != "HTTP/1.0") { res.status = 505; res.reason = "HTTP Version Not Supported"; res.body = "505 HTTP Version Not Supported\n"; return finish(); }
    if (!security::isMethodAllowed(req.method)) { res.status = 405; res.reason = "Method Not Allowed"; res.body = "405 Method Not Allowed\n"; return finish(); }

    res.headOnly = req.method == "HEAD";
    std::string host; auto hostIt = req.headers.find("Host"); if (hostIt != req.headers.end()) host = hostIt->second;
    auto documentRoot = selectDocumentRoot(host);
    auto safePath = security::resolveSafePath(documentRoot, req.target);
    if (!safePath.has_value()) { res.status = 403; res.reason = "Forbidden"; res.body = "403 Forbidden\n"; return finish(); }

    HtaccessRules rules;
    if (config_.enableHtaccess) {
        rules = Htaccess::evaluate(documentRoot, *safePath, config_.htaccessFilename, config_.maxHtaccessDepth);
        for (const auto& [k, v] : rules.responseHeaders) res.extraHeaders[k] = v;
        if (rules.denyAll) { res.status = 403; res.reason = "Forbidden"; res.body = "403 Forbidden\n"; return finish(); }
    }

    if (!fs::exists(*safePath) || !fs::is_regular_file(*safePath)) { res.status = 404; res.reason = "Not Found"; res.body = "404 Not Found\n"; return finish(); }
    if (safePath->filename() == config_.htaccessFilename) { res.status = 403; res.reason = "Forbidden"; res.body = "403 Forbidden\n"; return finish(); }

    if (config_.enablePhp && safePath->extension() == config_.phpExtension) {
        int phpStatus = 500;
        res.body = handlePhpScript(*safePath, req.target, req.method, tls, phpStatus);
        res.status = phpStatus;
        res.reason = phpStatus == 200 ? "OK" : "PHP FastCGI Error";
        res.contentType = "text/html; charset=utf-8";
        return finish();
    }

    res.contentType = mimeType(*safePath);
    res.body = readFile(*safePath);
    return finish();
}

fs::path Server::selectDocumentRoot(const std::string& host) const {
    auto normalized = security::normalizeHost(host);
    auto it = config_.virtualHosts.find(normalized);
    if (it != config_.virtualHosts.end()) return it->second;
    return config_.defaultRoot;
}

std::string Server::mimeType(const fs::path& path) const {
    auto ext = path.extension().string();
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".txt") return "text/plain; charset=utf-8";
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
    int& statusOut
) {
    try {
        std::string uri = requestUri;
        std::string query;
        auto q = uri.find('?');
        if (q != std::string::npos) {
            query = uri.substr(q + 1);
            uri = uri.substr(0, q);
        }

        FastCgiClient client(
            io_,
            config_.phpFastcgiHost,
            config_.phpFastcgiPort,
            config_.phpFastcgiConnectTimeoutSeconds,
            config_.phpFastcgiReadTimeoutSeconds
        );

        FastCgiRequest fcgi;
        fcgi.scriptFilename = scriptPath;
        fcgi.requestMethod = method;
        fcgi.requestUri = uri;
        fcgi.queryString = query;
        fcgi.documentRoot = config_.defaultRoot.string();

        auto response = client.execute(fcgi);
        statusOut = response.status;
        return response.body;
    } catch (const std::exception& e) {
        logger_.error(std::string("php_fastcgi error=\"") + e.what() + "\"");
        statusOut = 502;
        return "502 Bad Gateway: PHP FastCGI unavailable\n";
    }
}
