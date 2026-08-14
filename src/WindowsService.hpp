#pragma once
#include <filesystem>

#if defined(_WIN32)
int runHydrogenHttpdWindowsService(const std::filesystem::path& configPath);
int installHydrogenHttpdWindowsService(
    const std::filesystem::path& executable,
    const std::filesystem::path& configPath
);
int uninstallHydrogenHttpdWindowsService();
#endif
