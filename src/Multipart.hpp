#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

struct MultipartLimits {
    std::size_t maxParts = 16;
    std::size_t maxFileBytes = 1024 * 1024;
    std::size_t maxFieldBytes = 16 * 1024;
};

struct MultipartSavedFile {
    std::string fieldName;
    std::string originalFilename;
    std::string storedFilename;
    std::filesystem::path storedPath;
    std::size_t size = 0;
};

struct MultipartResult {
    bool ok = false;
    int status = 400;
    std::string error;
    std::vector<MultipartSavedFile> files;
};

class Multipart {
public:
    static bool extractBoundary(const std::string& contentType, std::string& boundaryOut);

    static MultipartResult saveUploads(
        const std::string& body,
        const std::string& contentType,
        const std::filesystem::path& uploadDirectory,
        const MultipartLimits& limits
    );

    static MultipartResult saveUploadsFromSpoolFile(
        const std::filesystem::path& spoolFile,
        const std::string& contentType,
        const std::filesystem::path& uploadDirectory,
        const MultipartLimits& limits
    );

    static std::string sanitizeFilename(const std::string& input);
    static bool isSafeUploadFilename(const std::string& filename);

private:
    static std::string trim(std::string value);
    static std::string lowercase(std::string value);
};
