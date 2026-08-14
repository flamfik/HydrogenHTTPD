#pragma once
#include "Config.hpp"
#include "AuthRuntimeStore.hpp"
#include "Logger.hpp"
#include "RateLimiter.hpp"
#include "ThreadPool.hpp"
#include "SqliteModule.hpp"
#include "StrictHttp.hpp"
#include "StaticFileCache.hpp"
#include "Http.hpp"
#include "Multipart.hpp"
#include <boost/asio.hpp>
#if defined(HYDROGENHTTPD_ENABLE_TLS)
#include <boost/asio/ssl.hpp>
#endif
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

class Server {
public:
    Server(boost::asio::io_context& io, ServerConfig config);
    void run();
private:
    void acceptPlain();
#if defined(HYDROGENHTTPD_ENABLE_TLS)
    void acceptTls();
    void configureTlsContext();
#endif
    void handlePlainClient(boost::asio::ip::tcp::socket socket);
#if defined(HYDROGENHTTPD_ENABLE_TLS)
    void handleTlsClient(boost::asio::ip::tcp::socket socket);
#endif
    std::string readPlainRequest(boost::asio::ip::tcp::socket& socket);
#if defined(HYDROGENHTTPD_ENABLE_TLS)
    std::string readTlsRequest(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& stream);
#endif
    template <typename SyncReadStream>
    HttpResponse handleClientStream(SyncReadStream& stream, bool tls, const std::string& clientIp, int& statusOut, std::string& methodOut, std::string& targetOut);
    HttpResponse makeResponse(const std::string& rawRequest, bool tls, int& statusOut, std::string& methodOut, std::string& targetOut);
    std::filesystem::path selectDocumentRoot(const std::string& host) const;
    bool isForbiddenFile(const std::filesystem::path& documentRoot, const std::filesystem::path& path) const;
    std::string httpsRedirectLocation(const std::string& host, const std::string& target) const;
    std::string mimeType(const std::filesystem::path& path) const;
    std::string readFile(const std::filesystem::path& path) const;
    std::string handlePhpScript(const std::filesystem::path& scriptPath, const std::string& requestUri, const std::string& method, bool tls, const std::string& body, const std::string& contentType, int& statusOut);
    HttpResponse handleUploadRequest(const HttpRequest& req, HttpResponse res, int& statusOut);
    AuthDecision authorizeEndpoint(const HttpRequest& req, const std::string& requiredScope);
    HttpResponse unauthorizedResponse(bool tls, int& statusOut, bool invalidToken = false) const;
    HttpResponse forbiddenResponse(bool tls, int& statusOut) const;
    HttpResponse adminStatusResponse(bool tls, int& statusOut) const;
    void auditEndpointEvent(const std::string& event, const std::string& clientIp, const std::string& target, const std::string& requiredScope, const std::string& tokenName, int status);

    boost::asio::io_context& io_;
    boost::asio::ip::tcp::acceptor plainAcceptor_;
#if defined(HYDROGENHTTPD_ENABLE_TLS)
    std::unique_ptr<boost::asio::ssl::context> tlsContext_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> tlsAcceptor_;
#endif
    ServerConfig config_;
    AuthRuntimeStore authStore_;
    Logger logger_;
    RateLimiter rateLimiter_;
    ThreadPool workers_;
    StaticFileCache staticCache_;
    SqliteModule sqlite_;
    std::chrono::steady_clock::time_point startedAt_;
};
