#include "Htaccess.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

std::string Htaccess::trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

HtaccessRules Htaccess::parseFile(const fs::path& file) {
    HtaccessRules rules;

    std::ifstream input(file);
    if (!input) return rules;

    std::string line;
    while (std::getline(input, line)) {
        auto comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string first;
        ss >> first;

        if (first == "Require") {
            std::string second, third;
            ss >> second >> third;
            if (second == "all" && third == "denied") rules.denyAll = true;
            if (second == "all" && third == "granted") rules.denyAll = false;
        } else if (first == "Options") {
            std::string option;
            while (ss >> option) {
                if (option == "-Indexes") rules.optionsIndexesDisabled = true;
            }
        } else if (first == "Header") {
            std::string action, name;
            ss >> action >> name;
            if (action == "set" && !name.empty()) {
                std::string value;
                std::getline(ss, value);
                value = trim(value);
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.size() - 2);
                }

                // Minimal header-name validation.
                bool safeName = !name.empty();
                for (char c : name) {
                    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-')) {
                        safeName = false;
                        break;
                    }
                }

                // Prevent overriding framing/protocol-critical headers.
                if (safeName &&
                    name != "Content-Length" &&
                    name != "Connection" &&
                    name != "Transfer-Encoding" &&
                    name != "Server") {
                    rules.responseHeaders[name] = value;
                }
            }
        }
    }

    return rules;
}

HtaccessRules Htaccess::evaluate(
    const fs::path& documentRoot,
    const fs::path& requestedPath,
    const std::string& filename,
    std::size_t maxDepth
) {
    HtaccessRules result;

    fs::path root = fs::weakly_canonical(documentRoot);
    fs::path current = fs::is_directory(requestedPath)
        ? fs::weakly_canonical(requestedPath)
        : fs::weakly_canonical(requestedPath.parent_path());

    std::vector<fs::path> dirs;
    std::size_t depth = 0;

    while (depth++ < maxDepth) {
        auto rel = current.lexically_relative(root);
        if (rel.empty() || (!rel.empty() && rel.native().find("..") == 0)) break;
        dirs.push_back(current);
        if (current == root) break;
        current = current.parent_path();
    }

    std::reverse(dirs.begin(), dirs.end());

    for (const auto& dir : dirs) {
        auto file = dir / filename;
        if (!fs::exists(file) || !fs::is_regular_file(file)) continue;

        auto local = parseFile(file);
        if (local.denyAll) result.denyAll = true;
        else if (!local.denyAll) {
            // Require all granted in a deeper .htaccess can re-open a child dir.
            // This mirrors the simple override behavior in our limited model.
            result.denyAll = false;
        }

        if (local.optionsIndexesDisabled) result.optionsIndexesDisabled = true;

        for (const auto& [k, v] : local.responseHeaders) {
            result.responseHeaders[k] = v;
        }
    }

    return result;
}
