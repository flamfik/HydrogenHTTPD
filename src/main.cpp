#include "Application.hpp"

#if defined(_WIN32)
#include "WindowsService.hpp"
#endif

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifndef HYDROGENHTTPD_VERSION
#define HYDROGENHTTPD_VERSION "unknown"
#endif

namespace {
struct Arguments {
    std::filesystem::path configPath = "server.conf";
    bool checkConfig = false;
    bool showVersion = false;
    bool showHelp = false;
#if defined(_WIN32)
    bool service = false;
    bool installService = false;
    bool uninstallService = false;
#endif
};

Arguments parseArguments(int argc, char* argv[]) {
    Arguments args;

    for (int i = 1; i < argc; ++i) {
        const std::string value = argv[i];

        if ((value == "--config" || value == "-c") && i + 1 < argc) {
            args.configPath = argv[++i];
        } else if (value == "--check-config") {
            args.checkConfig = true;
        } else if (value == "--version" || value == "-v") {
            args.showVersion = true;
        } else if (value == "--help" || value == "-h") {
            args.showHelp = true;
#if defined(_WIN32)
        } else if (value == "--service") {
            args.service = true;
        } else if (value == "--install-service") {
            args.installService = true;
        } else if (value == "--uninstall-service") {
            args.uninstallService = true;
#endif
        } else if (!value.empty() && value.front() != '-') {
            // Backward compatibility with: hydrogen_httpd server.conf
            args.configPath = value;
        } else {
            throw std::runtime_error("Unknown or incomplete argument: " + value);
        }
    }

    return args;
}

void printHelp() {
    std::cout
        << "HydrogenHttpd " << HYDROGENHTTPD_VERSION << "\n\n"
        << "Usage:\n"
        << "  hydrogen_httpd --config <file>\n"
        << "  hydrogen_httpd --check-config --config <file>\n"
        << "  hydrogen_httpd --version\n"
#if defined(_WIN32)
        << "  hydrogen_httpd --service --config <file>\n"
        << "  hydrogen_httpd --install-service --config <file>\n"
        << "  hydrogen_httpd --uninstall-service\n"
#endif
        << "\nOptions:\n"
        << "  -c, --config <file>  Configuration file\n"
        << "      --check-config   Validate configuration and exit\n"
        << "  -v, --version        Print version\n"
        << "  -h, --help           Show this help\n";
}
}

int main(int argc, char* argv[]) {
    try {
        const Arguments args = parseArguments(argc, argv);

        if (args.showHelp) {
            printHelp();
            return 0;
        }

        if (args.showVersion) {
            std::cout << "HydrogenHttpd " << HYDROGENHTTPD_VERSION << "\n";
            return 0;
        }

#if defined(_WIN32)
        if (args.service) return runHydrogenHttpdWindowsService(args.configPath);
        if (args.installService) {
            return installHydrogenHttpdWindowsService(
                std::filesystem::absolute(argv[0]),
                std::filesystem::absolute(args.configPath)
            );
        }
        if (args.uninstallService) return uninstallHydrogenHttpdWindowsService();
#endif

        if (args.checkConfig) return checkHydrogenHttpdConfig(args.configPath);
        return runHydrogenHttpd(args.configPath);
    } catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << "\n";
        printHelp();
        return 2;
    }
}
