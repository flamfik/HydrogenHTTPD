#include "Auth.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
constexpr std::array<std::uint32_t, 64> K{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

std::uint32_t rotr(std::uint32_t value, std::uint32_t count) {
    return (value >> count) | (value << (32U - count));
}

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> values;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, delimiter)) values.push_back(trim(item));
    return values;
}

std::vector<std::string> splitScopes(const std::string& text) {
    std::vector<std::string> scopes;
    for (auto value : split(text, ',')) {
        value = lowercase(value);
        if (!value.empty()) scopes.push_back(value);
    }
    return scopes;
}

bool isHex64(const std::string& value) {
    if (value.size() != 64) return false;
    for (unsigned char c : value) {
        if (!std::isxdigit(c)) return false;
    }
    return true;
}

std::int64_t parseExpiry(const std::string& value) {
    const std::string normalized = lowercase(trim(value));
    if (normalized.empty() || normalized == "0" || normalized == "never") return 0;

    std::size_t consumed = 0;
    const auto parsed = std::stoll(normalized, &consumed, 10);
    if (consumed != normalized.size() || parsed < 0) {
        throw std::runtime_error("token expiry must be 0, never, or a Unix epoch timestamp");
    }
    return parsed;
}
}

std::string Auth::sha256Hex(const std::string& input) {
    std::vector<std::uint8_t> message(input.begin(), input.end());
    const std::uint64_t bitLength = static_cast<std::uint64_t>(message.size()) * 8U;

    message.push_back(0x80U);
    while ((message.size() % 64U) != 56U) message.push_back(0U);

    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>((bitLength >> shift) & 0xffU));
    }

    std::array<std::uint32_t, 8> state{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };

    for (std::size_t offset = 0; offset < message.size(); offset += 64U) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16U; ++i) {
            const std::size_t base = offset + i * 4U;
            words[i] = (static_cast<std::uint32_t>(message[base]) << 24U) |
                       (static_cast<std::uint32_t>(message[base + 1U]) << 16U) |
                       (static_cast<std::uint32_t>(message[base + 2U]) << 8U) |
                       static_cast<std::uint32_t>(message[base + 3U]);
        }

        for (std::size_t i = 16U; i < 64U; ++i) {
            const std::uint32_t s0 = rotr(words[i - 15U], 7U) ^ rotr(words[i - 15U], 18U) ^ (words[i - 15U] >> 3U);
            const std::uint32_t s1 = rotr(words[i - 2U], 17U) ^ rotr(words[i - 2U], 19U) ^ (words[i - 2U] >> 10U);
            words[i] = words[i - 16U] + s0 + words[i - 7U] + s1;
        }

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];

        for (std::size_t i = 0; i < 64U; ++i) {
            const std::uint32_t sigma1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
            const std::uint32_t choice = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + sigma1 + choice + K[i] + words[i];
            const std::uint32_t sigma0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = sigma0 + majority;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto word : state) output << std::setw(8) << word;
    return output.str();
}

bool Auth::constantTimeEquals(const std::string& left, const std::string& right) {
    const std::size_t maxSize = std::max(left.size(), right.size());
    std::size_t difference = left.size() ^ right.size();

    for (std::size_t i = 0; i < maxSize; ++i) {
        const unsigned char l = i < left.size() ? static_cast<unsigned char>(left[i]) : 0U;
        const unsigned char r = i < right.size() ? static_cast<unsigned char>(right[i]) : 0U;
        difference |= static_cast<std::size_t>(l ^ r);
    }
    return difference == 0U;
}

bool Auth::hasScope(const AuthTokenRule& token, const std::string& requiredScope) {
    const std::string normalized = lowercase(requiredScope);
    for (const auto& scope : token.scopes) {
        if (scope == "*" || scope == normalized) return true;
    }
    return false;
}

bool Auth::isExpired(const AuthTokenRule& token, std::int64_t nowEpochSeconds) {
    return token.expiresAtEpoch > 0 && nowEpochSeconds >= token.expiresAtEpoch;
}

AuthDecision Auth::authorize(
    const std::string& authorizationHeader,
    const std::vector<AuthTokenRule>& tokens,
    const std::string& legacyPlaintextToken,
    const std::string& requiredScope,
    std::int64_t nowEpochSeconds
) {
    if (authorizationHeader.empty()) return {AuthDecisionCode::Missing, {}};

    constexpr const char* prefix = "Bearer ";
    if (authorizationHeader.rfind(prefix, 0) != 0) return {AuthDecisionCode::Invalid, {}};

    const std::string presented = authorizationHeader.substr(7U);
    if (presented.empty()) return {AuthDecisionCode::Invalid, {}};

    const std::string presentedHash = sha256Hex(presented);

    for (const auto& token : tokens) {
        bool matched = false;
        if (!token.sha256Hex.empty()) {
            matched = constantTimeEquals(lowercase(token.sha256Hex), presentedHash);
        } else if (!token.plaintextToken.empty()) {
            matched = constantTimeEquals(token.plaintextToken, presented);
        }

        if (!matched) continue;
        if (isExpired(token, nowEpochSeconds)) return {AuthDecisionCode::Expired, token.name};
        if (!hasScope(token, requiredScope)) return {AuthDecisionCode::InsufficientScope, token.name};
        return {AuthDecisionCode::Allowed, token.name};
    }

    if (!legacyPlaintextToken.empty() && constantTimeEquals(legacyPlaintextToken, presented)) {
        return {AuthDecisionCode::Allowed, "legacy"};
    }

    return {AuthDecisionCode::Invalid, {}};
}

AuthTokenRule Auth::parseTokenRule(const std::string& name, const std::string& value, bool requireHashedSecret) {
    const auto parts = split(value, '|');
    if (parts.size() < 2U || parts.size() > 3U) {
        throw std::runtime_error("token rule must use: secret | scopes | expires_epoch");
    }

    AuthTokenRule rule;
    rule.name = trim(name);
    rule.scopes = splitScopes(parts[1]);
    rule.expiresAtEpoch = parts.size() == 3U ? parseExpiry(parts[2]) : 0;

    std::string secret = trim(parts[0]);
    constexpr const char* hashPrefix = "sha256:";
    if (secret.rfind(hashPrefix, 0) == 0) {
        rule.sha256Hex = lowercase(secret.substr(7U));
        if (!isHex64(rule.sha256Hex)) throw std::runtime_error("sha256 token hash must contain exactly 64 hexadecimal characters");
    } else {
        if (requireHashedSecret) throw std::runtime_error("secrets file tokens must use sha256:<64-hex-hash>");
        rule.plaintextToken = secret;
    }

    if (rule.name.empty()) throw std::runtime_error("token name cannot be empty");
    if (rule.scopes.empty()) throw std::runtime_error("token scopes cannot be empty");
    if (rule.plaintextToken.empty() && rule.sha256Hex.empty()) throw std::runtime_error("token secret cannot be empty");
    return rule;
}

std::vector<AuthTokenRule> Auth::loadSecretsFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open auth secrets file: " + path.string());

    std::vector<AuthTokenRule> tokens;
    std::string line;
    while (std::getline(file, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;

        const auto equals = line.find('=');
        if (equals == std::string::npos) throw std::runtime_error("invalid auth secrets line: " + line);

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1U));
        constexpr const char* tokenPrefix = "token.";
        if (key.rfind(tokenPrefix, 0) != 0) throw std::runtime_error("auth secrets keys must start with token.");

        tokens.push_back(parseTokenRule(key.substr(6U), value, true));
    }

    return tokens;
}
