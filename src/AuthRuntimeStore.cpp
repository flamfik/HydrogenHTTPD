#include "AuthRuntimeStore.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <unordered_set>

namespace {
std::int64_t nowEpochSeconds() {
    return static_cast<std::int64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
    );
}
}

AuthRuntimeStore::AuthRuntimeStore(
    std::vector<AuthTokenRule> inlineTokens,
    std::filesystem::path secretsFile,
    bool hotReload,
    unsigned int reloadIntervalSeconds
) : inlineTokens_(std::move(inlineTokens)),
    secretsFile_(std::move(secretsFile)),
    hotReload_(hotReload),
    reloadIntervalSeconds_(reloadIntervalSeconds) {
    if (!secretsFile_.empty()) {
        if (!std::filesystem::exists(secretsFile_)) {
            throw std::runtime_error("auth secrets file does not exist: " + secretsFile_.string());
        }
        fileTokens_ = Auth::loadSecretsFile(secretsFile_);
        lastWriteTime_ = std::filesystem::last_write_time(secretsFile_);
        hadFile_ = true;
    }

    activeTokens_ = combineAndValidate(inlineTokens_, fileTokens_);
    generation_ = 1;
    lastSuccessfulReloadEpoch_ = nowEpochSeconds();
    lastCheck_ = std::chrono::steady_clock::now();
}

std::vector<AuthTokenRule> AuthRuntimeStore::combineAndValidate(
    const std::vector<AuthTokenRule>& inlineTokens,
    const std::vector<AuthTokenRule>& fileTokens
) {
    std::vector<AuthTokenRule> combined;
    combined.reserve(inlineTokens.size() + fileTokens.size());

    std::unordered_set<std::string> names;
    auto append = [&](const std::vector<AuthTokenRule>& source) {
        for (const auto& token : source) {
            if (!names.insert(token.name).second) {
                throw std::runtime_error("duplicate auth token name across stores: " + token.name);
            }
            combined.push_back(token);
        }
    };

    append(inlineTokens);
    append(fileTokens);
    return combined;
}

bool AuthRuntimeStore::checkIntervalElapsed(std::chrono::steady_clock::time_point now) const {
    if (reloadIntervalSeconds_ == 0) return true;
    return now - lastCheck_ >= std::chrono::seconds(reloadIntervalSeconds_);
}

AuthReloadResult AuthRuntimeStore::refresh(bool force) {
    std::lock_guard<std::mutex> lock(mutex_);

    AuthReloadResult result;
    result.tokenCount = activeTokens_.size();
    result.generation = generation_;

    if (!hotReload_ && !force) return result;
    if (secretsFile_.empty()) return result;

    const auto now = std::chrono::steady_clock::now();
    if (!force && !checkIntervalElapsed(now)) return result;
    lastCheck_ = now;

    const bool exists = std::filesystem::exists(secretsFile_);
    if (!exists) {
        if (!hadFile_) return result;

        try {
            fileTokens_.clear();
            activeTokens_ = combineAndValidate(inlineTokens_, fileTokens_);
            hadFile_ = false;
            lastWriteTime_ = {};
            ++generation_;
            lastSuccessfulReloadEpoch_ = nowEpochSeconds();

            result.code = AuthReloadCode::Reloaded;
            result.message = "auth secrets file removed; file-backed tokens revoked";
            result.tokenCount = activeTokens_.size();
            result.generation = generation_;
            return result;
        } catch (const std::exception& e) {
            result.code = AuthReloadCode::Failed;
            result.message = e.what();
            return result;
        }
    }

    std::filesystem::file_time_type writeTime;
    try {
        writeTime = std::filesystem::last_write_time(secretsFile_);
    } catch (const std::exception& e) {
        result.code = AuthReloadCode::Failed;
        result.message = e.what();
        return result;
    }

    if (!force && hadFile_ && writeTime == lastWriteTime_) return result;

    try {
        auto newFileTokens = Auth::loadSecretsFile(secretsFile_);
        auto combined = combineAndValidate(inlineTokens_, newFileTokens);

        fileTokens_ = std::move(newFileTokens);
        activeTokens_ = std::move(combined);
        lastWriteTime_ = writeTime;
        hadFile_ = true;
        ++generation_;
        lastSuccessfulReloadEpoch_ = nowEpochSeconds();

        result.code = AuthReloadCode::Reloaded;
        result.message = "auth secrets reloaded";
        result.tokenCount = activeTokens_.size();
        result.generation = generation_;
        return result;
    } catch (const std::exception& e) {
        // Remember the failed file version so the server does not retry and
        // flood logs on every request. A subsequent file change will retry.
        lastWriteTime_ = writeTime;
        hadFile_ = true;
        result.code = AuthReloadCode::Failed;
        result.message = e.what();
        return result;
    }
}

AuthDecision AuthRuntimeStore::authorize(
    const std::string& authorizationHeader,
    const std::string& legacyPlaintextToken,
    const std::string& requiredScope,
    std::int64_t nowEpoch,
    AuthReloadResult* reloadResult
) {
    auto reload = refresh(false);
    if (reloadResult != nullptr) *reloadResult = reload;

    std::lock_guard<std::mutex> lock(mutex_);
    return Auth::authorize(
        authorizationHeader,
        activeTokens_,
        legacyPlaintextToken,
        requiredScope,
        nowEpoch
    );
}

std::size_t AuthRuntimeStore::tokenCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeTokens_.size();
}

std::uint64_t AuthRuntimeStore::generation() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generation_;
}

std::int64_t AuthRuntimeStore::lastSuccessfulReloadEpoch() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastSuccessfulReloadEpoch_;
}

bool AuthRuntimeStore::hotReloadEnabled() const {
    return hotReload_;
}
