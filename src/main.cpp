#include "Config.hpp"
#include "Server.hpp"
#include <boost/asio.hpp>
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    std::filesystem::path configPath = "server.conf";
    if (argc >= 2) configPath = argv[1];

    try {
        ServerConfig config = ConfigLoader::load(configPath);
        if (!std::filesystem::exists(config.defaultRoot)) { std::cerr << "Default document root does not exist: " << config.defaultRoot << "\n"; return 1; }
        for (const auto& [host, root] : config.virtualHosts) {
            if (!std::filesystem::exists(root)) { std::cerr << "Virtual host document root does not exist: " << host << " -> " << root << "\n"; return 1; }
        }
#if defined(HYDROGENHTTPD_ENABLE_TLS)
        if (config.enableTls) {
            if (!std::filesystem::exists(config.tlsCertFile)) { std::cerr << "TLS certificate does not exist: " << config.tlsCertFile << "\n"; return 1; }
            if (!std::filesystem::exists(config.tlsKeyFile)) { std::cerr << "TLS private key does not exist: " << config.tlsKeyFile << "\n"; return 1; }
        }
#else
        if (config.enableTls) { std::cerr << "TLS is enabled in config, but binary was built without OpenSSL support.\n"; return 1; }
#endif
        boost::asio::io_context io;
        Server server(io, config);
        server.run();
        io.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
