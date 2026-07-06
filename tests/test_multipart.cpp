#include "Multipart.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    std::string boundary;
    assert(Multipart::extractBoundary("multipart/form-data; boundary=abc123", boundary));
    assert(boundary == "abc123");
    assert(Multipart::extractBoundary("multipart/form-data; boundary=\"abc-123\"", boundary));
    assert(boundary == "abc-123");
    assert(!Multipart::extractBoundary("text/plain", boundary));
    assert(!Multipart::extractBoundary("multipart/form-data; boundary=bad\r\n", boundary));

    assert(Multipart::sanitizeFilename("../../evil.php") == "evil.php");
    assert(!Multipart::isSafeUploadFilename("evil.php"));
    assert(!Multipart::isSafeUploadFilename("x.phtml"));
    assert(!Multipart::isSafeUploadFilename("private.key"));
    assert(Multipart::isSafeUploadFilename("photo.jpg"));
    assert(Multipart::isSafeUploadFilename("notes.txt"));

    auto dir = fs::temp_directory_path() / "hydrogen_multipart_test";
    fs::remove_all(dir);

    std::string body =
        "--abc\r\n"
        "Content-Disposition: form-data; name=\"title\"\r\n"
        "\r\n"
        "hello\r\n"
        "--abc\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"notes.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "file-content\r\n"
        "--abc--\r\n";

    auto result = Multipart::saveUploads(body, "multipart/form-data; boundary=abc", dir, MultipartLimits{4, 1024, 1024});
    assert(result.ok);
    assert(result.files.size() == 1);
    assert(result.files[0].originalFilename == "notes.txt");
    assert(result.files[0].size == 12);
    assert(fs::exists(result.files[0].storedPath));

    auto blocked = Multipart::saveUploads(
        "--abc\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"shell.php\"\r\n"
        "\r\n"
        "<?php echo 1; ?>\r\n"
        "--abc--\r\n",
        "multipart/form-data; boundary=abc",
        dir,
        MultipartLimits{4, 1024, 1024}
    );
    assert(!blocked.ok);
    assert(blocked.status == 415);

    auto spool = dir / "spool.tmp";
    std::string spoolBody =
        "--xyz\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"stream.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "stream-content\r\n"
        "--xyz--\r\n";
    {
        std::ofstream out(spool, std::ios::binary | std::ios::trunc);
        out << spoolBody;
    }

    auto streamed = Multipart::saveUploadsFromSpoolFile(
        spool,
        "multipart/form-data; boundary=xyz",
        dir,
        MultipartLimits{4, 1024, 1024}
    );
    assert(streamed.ok);
    assert(streamed.files.size() == 1);
    assert(streamed.files[0].originalFilename == "stream.txt");
    assert(streamed.files[0].size == 14);
    assert(fs::exists(streamed.files[0].storedPath));

    fs::remove_all(dir);
    std::cout << "multipart tests passed\n";
    return 0;
}
