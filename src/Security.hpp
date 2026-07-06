#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace security {
std::optional<std::filesystem::path> resolveSafePath(const std::filesystem::path& documentRoot, const std::string& rawTarget);
bool isMethodAllowed(const std::string& method);
bool hasSuspiciousTarget(const std::string& target);
std::string urlDecode(const std::string& value);
std::string normalizeHost(std::string host);
}
