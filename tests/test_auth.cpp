#include "Auth.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    assert(Auth::sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(Auth::sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(Auth::constantTimeEquals("same", "same"));
    assert(!Auth::constantTimeEquals("same", "different"));

    AuthTokenRule upload;
    upload.name = "upload";
    upload.sha256Hex = Auth::sha256Hex("upload-secret");
    upload.scopes = {"upload"};

    AuthTokenRule admin;
    admin.name = "admin";
    admin.sha256Hex = Auth::sha256Hex("admin-secret");
    admin.scopes = {"upload", "admin"};

    AuthTokenRule expired;
    expired.name = "expired";
    expired.sha256Hex = Auth::sha256Hex("expired-secret");
    expired.scopes = {"admin"};
    expired.expiresAtEpoch = 100;

    std::vector<AuthTokenRule> tokens{upload, admin, expired};

    auto missing = Auth::authorize("", tokens, "", "upload", 50);
    assert(missing.code == AuthDecisionCode::Missing);

    auto invalid = Auth::authorize("Bearer wrong", tokens, "", "upload", 50);
    assert(invalid.code == AuthDecisionCode::Invalid);

    auto allowedUpload = Auth::authorize("Bearer upload-secret", tokens, "", "upload", 50);
    assert(allowedUpload.allowed());
    assert(allowedUpload.tokenName == "upload");

    auto deniedAdmin = Auth::authorize("Bearer upload-secret", tokens, "", "admin", 50);
    assert(deniedAdmin.code == AuthDecisionCode::InsufficientScope);
    assert(deniedAdmin.tokenName == "upload");

    auto allowedAdmin = Auth::authorize("Bearer admin-secret", tokens, "", "admin", 50);
    assert(allowedAdmin.allowed());
    assert(allowedAdmin.tokenName == "admin");

    auto expiredAdmin = Auth::authorize("Bearer expired-secret", tokens, "", "admin", 100);
    assert(expiredAdmin.code == AuthDecisionCode::Expired);
    assert(expiredAdmin.tokenName == "expired");

    auto legacy = Auth::authorize("Bearer legacy-secret", tokens, "legacy-secret", "admin", 50);
    assert(legacy.allowed());
    assert(legacy.tokenName == "legacy");

    fs::path secretsPath = fs::temp_directory_path() / "hydrogen_auth_test.tokens";
    std::ofstream(secretsPath)
        << "token.upload = sha256:" << Auth::sha256Hex("upload-secret") << " | upload | never\n"
        << "token.admin = sha256:" << Auth::sha256Hex("admin-secret") << " | upload,admin | 2000000000\n";

    auto loaded = Auth::loadSecretsFile(secretsPath);
    assert(loaded.size() == 2);
    assert(loaded[0].name == "upload");
    assert(loaded[0].plaintextToken.empty());
    assert(loaded[0].sha256Hex == Auth::sha256Hex("upload-secret"));
    assert(loaded[1].expiresAtEpoch == 2000000000);

    fs::remove(secretsPath);
    std::cout << "auth tests passed\n";
    return 0;
}
