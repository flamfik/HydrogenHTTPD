#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct AuthTokenRule {
    std::string name;
    std::string plaintextToken;
    std::string sha256Hex;
    std::vector<std::string> scopes;
    std::int64_t expiresAtEpoch = 0;
};

enum class AuthDecisionCode {
    Allowed,
    Missing,
    Invalid,
    Expired,
    InsufficientScope
};

struct AuthDecision {
    AuthDecisionCode code = AuthDecisionCode::Invalid;
    std::string tokenName;

    bool allowed() const { return code == AuthDecisionCode::Allowed; }
};

class Auth {
public:
    static std::string sha256Hex(const std::string& input);
    static bool constantTimeEquals(const std::string& left, const std::string& right);
    static bool hasScope(const AuthTokenRule& token, const std::string& requiredScope);
    static bool isExpired(const AuthTokenRule& token, std::int64_t nowEpochSeconds);

    static AuthDecision authorize(
        const std::string& authorizationHeader,
        const std::vector<AuthTokenRule>& tokens,
        const std::string& legacyPlaintextToken,
        const std::string& requiredScope,
        std::int64_t nowEpochSeconds
    );

    static std::vector<AuthTokenRule> loadSecretsFile(const std::filesystem::path& path);
    static AuthTokenRule parseTokenRule(const std::string& name, const std::string& value, bool requireHashedSecret);
};
