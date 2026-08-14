#include "Application.hpp"
#include "Config.hpp"
#include "Server.hpp"

#include <boost/asio.hpp>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

namespace {
ServerConfig loadAndValidate(const std::filesystem::path& configPath) {
    ServerConfig config = ConfigLoader::load(configPath);

    if (!std::filesystem::exists(config.defaultRoot)) {
        throw std::runtime_error("Default document root does not exist: " + config.defaultRoot.string());
    }

    for (const auto& [host, root] : config.virtualHosts) {
        if (!std::filesystem::exists(root)) {
            throw std::runtime_error(
                "Virtual host document root does not exist: " + host + " -> " + root.string()
            );
        }
    }

#if defined(HYDROGENHTTPD_ENABLE_TLS)
    if (config.enableTls) {
        if (!std::filesystem::exists(config.tlsCertFile)) {
            throw std::runtime_error("TLS certificate does not exist: " + config.tlsCertFile.string());
        }
        if (!std::filesystem::exists(config.tlsKeyFile)) {
            throw std::runtime_error("TLS private key does not exist: " + config.tlsKeyFile.string());
        }
    }
#else
    if (config.enableTls) {
        throw std::runtime_error("TLS is enabled in config, but binary was built without OpenSSL support.");
    }
#endif

    return config;
}
}

int checkHydrogenHttpdConfig(const std::filesystem::path& configPath) {
    try {
        const auto config = loadAndValidate(configPath);
        std::cout
            << "Configuration OK\n"
            << "Config: " << std::filesystem::absolute(configPath) << "\n"
            << "Document root: " << config.defaultRoot << "\n"
            << "HTTP port: " << config.port << "\n"
            << "TLS enabled: " << (config.enableTls ? "yes" : "no") << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Configuration error: " << e.what() << "\n";
        return 1;
    }
}

int runHydrogenHttpd(
    const std::filesystem::path& configPath,
    const std::function<void(boost::asio::io_context&)>& onIoReady
) {
    try {
        ServerConfig config = loadAndValidate(configPath);

        boost::asio::io_context io(static_cast<int>(config.ioThreads));

#if !defined(_WIN32)
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](const boost::system::error_code& ec, int) {
            if (!ec) io.stop();
        });
#endif

        if (onIoReady) onIoReady(io);

        Server server(io, config);
        server.run();

        std::vector<std::thread> ioWorkers;
        ioWorkers.reserve(config.ioThreads > 0 ? config.ioThreads - 1 : 0);
        for (std::size_t i = 1; i < config.ioThreads; ++i) {
            ioWorkers.emplace_back([&io] { io.run(); });
        }

        io.run();

        for (auto& worker : ioWorkers) {
            if (worker.joinable()) worker.join();
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
