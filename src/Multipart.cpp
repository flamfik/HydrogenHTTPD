#include "Multipart.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
std::string basenameOnly(std::string value) {
    auto slash = value.find_last_of("/\\");
    if (slash != std::string::npos) value = value.substr(slash + 1);
    return value;
}

bool validBoundaryChar(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    if (std::isalnum(u)) return true;
    const std::string allowed = "'()+_,-./:=?";
    return allowed.find(c) != std::string::npos;
}

std::map<std::string, std::string> parsePartHeaders(const std::string& raw) {
    std::map<std::string, std::string> headers;
    std::size_t start = 0;

    while (start < raw.size()) {
        auto end = raw.find("\r\n", start);
        if (end == std::string::npos) end = raw.size();

        std::string line = raw.substr(start, end - start);
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();

            headers[name] = value;
        }

        if (end == raw.size()) break;
        start = end + 2;
    }

    return headers;
}

std::string dispositionValue(const std::string& disposition, const std::string& key) {
    std::string needle = key + "=\"";
    auto pos = disposition.find(needle);
    if (pos != std::string::npos) {
        pos += needle.size();
        auto end = disposition.find('"', pos);
        if (end == std::string::npos) return {};
        return disposition.substr(pos, end - pos);
    }

    needle = key + "=";
    pos = disposition.find(needle);
    if (pos != std::string::npos) {
        pos += needle.size();
        auto end = disposition.find(';', pos);
        if (end == std::string::npos) end = disposition.size();
        return disposition.substr(pos, end - pos);
    }

    return {};
}

std::string uniquePrefix() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto count = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    return std::to_string(count);
}
}

std::string Multipart::trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

std::string Multipart::lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool Multipart::extractBoundary(const std::string& contentType, std::string& boundaryOut) {
    std::string lower = lowercase(contentType);
    if (lower.find("multipart/form-data") == std::string::npos) return false;

    auto pos = lower.find("boundary=");
    if (pos == std::string::npos) return false;
    pos += 9;

    std::string boundary = contentType.substr(pos);
    auto semi = boundary.find(';');
    if (semi != std::string::npos) boundary = boundary.substr(0, semi);
    if (boundary.find('\r') != std::string::npos || boundary.find('\n') != std::string::npos) return false;
    boundary = trim(boundary);

    if (boundary.size() >= 2 && boundary.front() == '"' && boundary.back() == '"') {
        boundary = boundary.substr(1, boundary.size() - 2);
    }

    if (boundary.empty() || boundary.size() > 70) return false;
    for (char c : boundary) {
        if (!validBoundaryChar(c)) return false;
    }

    boundaryOut = boundary;
    return true;
}

std::string Multipart::sanitizeFilename(const std::string& input) {
    std::string name = basenameOnly(input);
    std::string out;
    out.reserve(name.size());

    for (unsigned char c : name) {
        if (std::isalnum(c) || c == '.' || c == '_' || c == '-') out.push_back(static_cast<char>(c));
        else out.push_back('_');
    }

    while (!out.empty() && (out.front() == '.' || out.front() == ' ')) out.erase(out.begin());
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();

    if (out.empty()) out = "upload.bin";
    if (out.size() > 96) out = out.substr(out.size() - 96);
    if (!out.empty() && out.front() == '.') out = "upload_" + out;
    return out;
}

bool Multipart::isSafeUploadFilename(const std::string& filename) {
    auto safe = sanitizeFilename(filename);
    auto lower = lowercase(safe);
    if (lower.empty()) return false;

    if (lower == ".htaccess" || lower == "server.conf" || lower == "server.production.conf") return false;

    static const std::set<std::string> blockedExt{
        ".php", ".phtml", ".php3", ".php4", ".php5", ".phar",
        ".cgi", ".pl", ".py", ".rb", ".lua",
        ".sh", ".bash", ".zsh", ".fish",
        ".bat", ".cmd", ".ps1", ".vbs",
        ".exe", ".dll", ".so", ".dylib",
        ".htaccess", ".conf", ".ini", ".db", ".sqlite", ".sqlite3",
        ".key", ".pem", ".crt", ".csr", ".log"
    };

    fs::path path(lower);
    return blockedExt.count(path.extension().string()) == 0;
}

MultipartResult Multipart::saveUploads(
    const std::string& body,
    const std::string& contentType,
    const fs::path& uploadDirectory,
    const MultipartLimits& limits
) {
    MultipartResult result;
    std::string boundary;

    if (!extractBoundary(contentType, boundary)) {
        result.status = 415;
        result.error = "Unsupported Media Type";
        return result;
    }

    if (limits.maxParts == 0 || limits.maxFileBytes == 0) {
        result.status = 500;
        result.error = "Upload limits are invalid";
        return result;
    }

    fs::create_directories(uploadDirectory);

    std::string delimiter = "--" + boundary;
    std::size_t pos = body.find(delimiter);
    if (pos == std::string::npos) {
        result.status = 400;
        result.error = "Multipart boundary not found";
        return result;
    }

    std::size_t partCount = 0;

    while (pos != std::string::npos) {
        pos += delimiter.size();

        if (body.compare(pos, 2, "--") == 0) break;
        if (body.compare(pos, 2, "\r\n") != 0) {
            result.status = 400;
            result.error = "Malformed multipart boundary";
            return result;
        }
        pos += 2;

        std::size_t next = body.find("\r\n" + delimiter, pos);
        if (next == std::string::npos) {
            result.status = 400;
            result.error = "Multipart terminator missing";
            return result;
        }

        std::string part = body.substr(pos, next - pos);
        pos = next + 2;

        if (++partCount > limits.maxParts) {
            result.status = 413;
            result.error = "Too many multipart parts";
            return result;
        }

        auto headerEnd = part.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            result.status = 400;
            result.error = "Multipart part headers missing";
            return result;
        }

        auto headers = parsePartHeaders(part.substr(0, headerEnd));
        std::string content = part.substr(headerEnd + 4);

        auto dispIt = headers.find("content-disposition");
        if (dispIt == headers.end()) continue;

        std::string fieldName = dispositionValue(dispIt->second, "name");
        std::string originalFilename = dispositionValue(dispIt->second, "filename");

        if (originalFilename.empty()) {
            if (content.size() > limits.maxFieldBytes) {
                result.status = 413;
                result.error = "Multipart field too large";
                return result;
            }
            continue;
        }

        if (content.size() > limits.maxFileBytes) {
            result.status = 413;
            result.error = "Uploaded file too large";
            return result;
        }

        std::string clean = sanitizeFilename(originalFilename);
        if (!isSafeUploadFilename(clean)) {
            result.status = 415;
            result.error = "Upload filename or extension is not allowed";
            return result;
        }

        std::string storedName = uniquePrefix() + "_" + clean;
        fs::path storedPath = uploadDirectory / storedName;

        std::ofstream out(storedPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            result.status = 500;
            result.error = "Failed to create uploaded file";
            return result;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.close();

        result.files.push_back(MultipartSavedFile{
            fieldName,
            originalFilename,
            storedName,
            storedPath,
            content.size()
        });
    }

    result.ok = true;
    result.status = 200;
    return result;
}


MultipartResult Multipart::saveUploadsFromSpoolFile(
    const fs::path& spoolFile,
    const std::string& contentType,
    const fs::path& uploadDirectory,
    const MultipartLimits& limits
) {
    MultipartResult result;
    std::string boundary;

    if (!extractBoundary(contentType, boundary)) {
        result.status = 415;
        result.error = "Unsupported Media Type";
        return result;
    }

    if (limits.maxParts == 0 || limits.maxFileBytes == 0) {
        result.status = 500;
        result.error = "Upload limits are invalid";
        return result;
    }

    std::ifstream in(spoolFile, std::ios::binary);
    if (!in) {
        result.status = 500;
        result.error = "Failed to open upload spool";
        return result;
    }

    fs::create_directories(uploadDirectory);

    const std::string delimiter = "--" + boundary;
    const std::string finalDelimiter = delimiter + "--";

    std::string line;
    auto getlineClean = [&](std::string& out) -> bool {
        if (!std::getline(in, out)) return false;
        if (!out.empty() && out.back() == '\r') out.pop_back();
        return true;
    };

    if (!getlineClean(line) || line != delimiter) {
        result.status = 400;
        result.error = "Multipart boundary not found";
        return result;
    }

    std::size_t partCount = 0;

    while (true) {
        if (++partCount > limits.maxParts) {
            result.status = 413;
            result.error = "Too many multipart parts";
            return result;
        }

        std::string rawHeaders;
        while (getlineClean(line)) {
            if (line.empty()) break;
            rawHeaders += line + "\r\n";
        }

        if (!in && rawHeaders.empty()) {
            result.status = 400;
            result.error = "Multipart part headers missing";
            return result;
        }

        auto headers = parsePartHeaders(rawHeaders);
        auto dispIt = headers.find("content-disposition");

        std::string fieldName;
        std::string originalFilename;
        bool isFile = false;

        if (dispIt != headers.end()) {
            fieldName = dispositionValue(dispIt->second, "name");
            originalFilename = dispositionValue(dispIt->second, "filename");
            isFile = !originalFilename.empty();
        }

        std::ofstream out;
        fs::path storedPath;
        std::string storedName;
        std::size_t bytesWritten = 0;
        std::size_t fieldBytes = 0;

        if (isFile) {
            std::string clean = sanitizeFilename(originalFilename);
            if (!isSafeUploadFilename(clean)) {
                result.status = 415;
                result.error = "Upload filename or extension is not allowed";
                return result;
            }

            storedName = uniquePrefix() + "_" + clean;
            storedPath = uploadDirectory / storedName;
            out.open(storedPath, std::ios::binary | std::ios::trunc);
            if (!out) {
                result.status = 500;
                result.error = "Failed to create uploaded file";
                return result;
            }
        }

        bool hasPending = false;
        std::string pending;

        while (getlineClean(line)) {
            if (line == delimiter || line == finalDelimiter) {
                if (hasPending) {
                    if (isFile) {
                        if (bytesWritten + pending.size() > limits.maxFileBytes) {
                            result.status = 413;
                            result.error = "Uploaded file too large";
                            return result;
                        }
                        out.write(pending.data(), static_cast<std::streamsize>(pending.size()));
                        bytesWritten += pending.size();
                    } else {
                        if (fieldBytes + pending.size() > limits.maxFieldBytes) {
                            result.status = 413;
                            result.error = "Multipart field too large";
                            return result;
                        }
                        fieldBytes += pending.size();
                    }
                }

                if (isFile) {
                    out.close();
                    result.files.push_back(MultipartSavedFile{
                        fieldName,
                        originalFilename,
                        storedName,
                        storedPath,
                        bytesWritten
                    });
                }

                if (line == finalDelimiter) {
                    result.ok = true;
                    result.status = 200;
                    return result;
                }

                break;
            }

            if (hasPending) {
                if (isFile) {
                    if (bytesWritten + pending.size() + 1 > limits.maxFileBytes) {
                        result.status = 413;
                        result.error = "Uploaded file too large";
                        return result;
                    }
                    out.write(pending.data(), static_cast<std::streamsize>(pending.size()));
                    out.put('\n');
                    bytesWritten += pending.size() + 1;
                } else {
                    if (fieldBytes + pending.size() + 1 > limits.maxFieldBytes) {
                        result.status = 413;
                        result.error = "Multipart field too large";
                        return result;
                    }
                    fieldBytes += pending.size() + 1;
                }
            }

            pending = line;
            hasPending = true;
        }

        if (!in) {
            result.status = 400;
            result.error = "Multipart terminator missing";
            return result;
        }
    }
}
