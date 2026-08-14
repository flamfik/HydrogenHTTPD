#pragma once
#include <filesystem>
#include <functional>

namespace boost::asio {
class io_context;
}

int runHydrogenHttpd(
    const std::filesystem::path& configPath,
    const std::function<void(boost::asio::io_context&)>& onIoReady = {}
);

int checkHydrogenHttpdConfig(const std::filesystem::path& configPath);
