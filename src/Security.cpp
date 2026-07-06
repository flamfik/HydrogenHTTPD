#include "Security.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace fs = std::filesystem;

namespace security {


static bool containsDangerousResidualEncoding(const std::string& value) {
    std::string lowerValue = value;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    // After one URL decode pass, these remaining encodings indicate double-encoded
    // traversal/separator attempts such as %252e%252e or %252f.
    return lowerValue.find("%2e") != std::string::npos ||
           lowerValue.find("%2f") != std::string::npos ||
           lowerValue.find("%5c") != std::string::npos;
}

static bool startsWithPath(const fs::path& path, const fs::path& root) {
    auto p = fs::weakly_canonical(path);
    auto r = fs::weakly_canonical(root);
    auto pit = p.begin();
    auto rit = r.begin();
    for (; rit != r.end(); ++rit, ++pit) {
        if (pit == p.end() || *pit != *rit) return false;
    }
    return true;
}

std::string urlDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const char hex[3] = { value[i + 1], value[i + 2], '\0' };
            char* end = nullptr;
            long decoded = std::strtol(hex, &end, 16);
            if (end && *end == '\0') {
                result.push_back(static_cast<char>(decoded));
                i += 2;
            } else result.push_back(value[i]);
        } else result.push_back(value[i]);
    }
    return result;
}

bool isMethodAllowed(const std::string& method) { return method == "GET" || method == "HEAD" || method == "POST"; }

bool hasSuspiciousTarget(const std::string& target) {
    if (target.empty()) return true;
    if (target.size() > 2048) return true;
    if (target[0] != '/') return true;
    if (target.find('\0') != std::string::npos) return true;
    if (target.find('\\') != std::string::npos) return true;
    return false;
}

std::optional<fs::path> resolveSafePath(const fs::path& documentRoot, const std::string& rawTarget) {
    if (hasSuspiciousTarget(rawTarget)) return std::nullopt;
    if (containsDangerousResidualEncoding(rawTarget)) return std::nullopt;
    std::string target = rawTarget;
    auto queryPos = target.find('?');
    if (queryPos != std::string::npos) target = target.substr(0, queryPos);
    target = urlDecode(target);
    if (containsDangerousResidualEncoding(target)) return std::nullopt;
    if (target.find('\0') != std::string::npos) return std::nullopt;
    if (target.find('\\') != std::string::npos) return std::nullopt;
    while (!target.empty() && target.front() == '/') target.erase(target.begin());
    if (target.empty()) target = "index.html";

    fs::path candidate = documentRoot / fs::path(target);
    if (fs::is_directory(candidate)) candidate /= "index.html";

    auto rootCanonical = fs::weakly_canonical(documentRoot);
    auto candidateCanonical = fs::weakly_canonical(candidate);
    if (!startsWithPath(candidateCanonical, rootCanonical)) return std::nullopt;
    return candidateCanonical;
}

std::string normalizeHost(std::string host) {
    auto colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0, colon);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    while (!host.empty() && std::isspace(static_cast<unsigned char>(host.front()))) host.erase(host.begin());
    while (!host.empty() && std::isspace(static_cast<unsigned char>(host.back()))) host.pop_back();
    return host;
}
}
