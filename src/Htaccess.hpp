#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>

struct HtaccessRules {
    bool denyAll = false;
    bool optionsIndexesDisabled = true;
    std::unordered_map<std::string, std::string> responseHeaders;
};

class Htaccess {
public:
    static HtaccessRules evaluate(
        const std::filesystem::path& documentRoot,
        const std::filesystem::path& requestedPath,
        const std::string& filename,
        std::size_t maxDepth
    );

    static HtaccessRules parseFile(const std::filesystem::path& file);

private:
    static std::string trim(std::string value);
};
