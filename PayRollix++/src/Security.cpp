#include "Security.h"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

namespace security {
namespace {
void Check(NTSTATUS status) {
    if (status < 0) throw std::runtime_error("Windows password cryptography failed.");
}
std::string Hex(std::vector<unsigned char> const& bytes) {
    static char const* digits = "0123456789abcdef";
    std::string text;
    for (auto b : bytes) { text += digits[b >> 4]; text += digits[b & 15]; }
    return text;
}
std::vector<unsigned char> Unhex(std::string const& text) {
    if (text.size() % 2) throw std::runtime_error("Invalid stored password hash.");
    std::vector<unsigned char> out;
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        throw std::runtime_error("Invalid stored password hash.");
    };
    for (size_t i = 0; i < text.size(); i += 2)
        out.push_back(static_cast<unsigned char>((digit(text[i]) << 4) | digit(text[i+1])));
    return out;
}
struct Algorithm {
    BCRYPT_ALG_HANDLE value = nullptr;
    explicit Algorithm(ULONG flags) { Check(BCryptOpenAlgorithmProvider(&value, BCRYPT_SHA256_ALGORITHM, nullptr, flags)); }
    ~Algorithm() { if (value) BCryptCloseAlgorithmProvider(value, 0); }
};
std::vector<unsigned char> Derive(std::string const& p, std::vector<unsigned char> const& salt) {
    Algorithm algorithm(BCRYPT_ALG_HANDLE_HMAC_FLAG);
    std::vector<unsigned char> result(32);
    Check(BCryptDeriveKeyPBKDF2(algorithm.value,
        reinterpret_cast<PUCHAR>(const_cast<char*>(p.data())), static_cast<ULONG>(p.size()),
        const_cast<PUCHAR>(salt.data()), static_cast<ULONG>(salt.size()), 600000,
        result.data(), static_cast<ULONG>(result.size()), 0));
    return result;
}
bool Equal(std::string const& a, std::string const& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}
}
std::string Hash(std::string const& password) {
    std::vector<unsigned char> salt(16);
    Check(BCryptGenRandom(nullptr, salt.data(), static_cast<ULONG>(salt.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    return "pbkdf2$600000$" + Hex(salt) + "$" + Hex(Derive(password, salt));
}
bool Verify(std::string const& password, std::string const& stored) {
    if (stored.rfind("pbkdf2$600000$", 0) == 0) {
        auto separator = stored.find('$', 14);
        // Prefix length is 14; salt immediately follows it.
        if (separator == std::string::npos) return false;
        auto salt = Unhex(stored.substr(14, separator - 14));
        if (salt.size() != 16 || stored.size() - separator - 1 != 64) return false;
        return Equal(Hex(Derive(password, salt)), stored.substr(separator + 1));
    }
    if (stored.size() != 64) return false;
    Algorithm algorithm(0);
    std::vector<unsigned char> result(32);
    Check(BCryptHash(algorithm.value, nullptr, 0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())), static_cast<ULONG>(password.size()),
        result.data(), static_cast<ULONG>(result.size())));
    return Equal(Hex(result), stored);
}
} // namespace security
