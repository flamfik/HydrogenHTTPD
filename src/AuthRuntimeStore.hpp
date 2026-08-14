#pragma once

#include "Auth.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

enum class AuthReloadCode {
    NoChange,
    Reloaded,
    Failed
};

struct AuthReloadResult {
    AuthReloadCode code = AuthReloadCode::NoChange;
    std::string message;
    std::size_t tokenCount = 0;
    std::uint64_t generation = 0;
};

class AuthRuntimeStore {
public:
    AuthRuntimeStore(
        std::vector<AuthTokenRule> inlineTokens,
        std::filesystem::path secretsFile,
        bool hotReload,
        unsigned int reloadIntervalSeconds
    );

    AuthDecision authorize(
        const std::string& authorizationHeader,
        const std::string& legacyPlaintextToken,
        const std::string& requiredScope,
        std::int64_t nowEpochSeconds,
        AuthReloadResult* reloadResult = nullptr
    );

    AuthReloadResult refresh(bool force = false);

    std::size_t tokenCount() const;
    std::uint64_t generation() const;
    std::int64_t lastSuccessfulReloadEpoch() const;
    bool hotReloadEnabled() const;

private:
    static std::vector<AuthTokenRule> combineAndValidate(
        const std::vector<AuthTokenRule>& inlineTokens,
        const std::vector<AuthTokenRule>& fileTokens
    );

    bool checkIntervalElapsed(std::chrono::steady_clock::time_point now) const;

    mutable std::mutex mutex_;
    std::vector<AuthTokenRule> inlineTokens_;
    std::vector<AuthTokenRule> fileTokens_;
    std::vector<AuthTokenRule> activeTokens_;
    std::filesystem::path secretsFile_;
    bool hotReload_ = true;
    unsigned int reloadIntervalSeconds_ = 0;
    std::chrono::steady_clock::time_point lastCheck_{};
    std::filesystem::file_time_type lastWriteTime_{};
    bool hadFile_ = false;
    std::uint64_t generation_ = 0;
    std::int64_t lastSuccessfulReloadEpoch_ = 0;
};
