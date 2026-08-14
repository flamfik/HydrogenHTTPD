#include "AuthRuntimeStore.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace {
void writeTokenFile(const fs::path& path, const std::string& tokenName, const std::string& plaintext) {
    std::ofstream(path, std::ios::trunc)
        << "token." << tokenName
        << " = sha256:" << Auth::sha256Hex(plaintext)
        << " | admin | never\n";
}
}

int main() {
    const auto temp = fs::temp_directory_path() / "hydrogen_auth_runtime_store.tokens";
    fs::remove(temp);

    writeTokenFile(temp, "old", "old-secret");

    AuthRuntimeStore store({}, temp, true, 0);
    assert(store.tokenCount() == 1);
    assert(store.generation() == 1);

    auto oldAllowed = store.authorize("Bearer old-secret", "", "admin", 100);
    assert(oldAllowed.allowed());
    assert(oldAllowed.tokenName == "old");

    writeTokenFile(temp, "new", "new-secret");
    auto reloaded = store.refresh(true);
    assert(reloaded.code == AuthReloadCode::Reloaded);
    assert(reloaded.generation == 2);
    assert(reloaded.tokenCount == 1);

    auto oldRevoked = store.authorize("Bearer old-secret", "", "admin", 100);
    assert(oldRevoked.code == AuthDecisionCode::Invalid);

    auto newAllowed = store.authorize("Bearer new-secret", "", "admin", 100);
    assert(newAllowed.allowed());
    assert(newAllowed.tokenName == "new");

    std::ofstream(temp, std::ios::trunc) << "this is not a token rule\n";
    auto failed = store.refresh(true);
    assert(failed.code == AuthReloadCode::Failed);
    assert(store.generation() == 2);

    auto lastKnownGood = store.authorize("Bearer new-secret", "", "admin", 100);
    assert(lastKnownGood.allowed());

    fs::remove(temp);
    auto removed = store.refresh(true);
    assert(removed.code == AuthReloadCode::Reloaded);
    assert(removed.generation == 3);
    assert(store.tokenCount() == 0);

    auto fileTokensRevoked = store.authorize("Bearer new-secret", "", "admin", 100);
    assert(fileTokensRevoked.code == AuthDecisionCode::Invalid);

    fs::remove(temp);
    std::cout << "auth runtime store tests passed\n";
    return 0;
}
