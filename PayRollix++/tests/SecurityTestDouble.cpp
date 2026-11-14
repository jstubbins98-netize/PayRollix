// Test double for portable DATABASE tests, never linked into PayrollApp.
// Native Windows tests link Security.cpp and exercise the real BCrypt implementation.
#include "Security.h"
namespace security {
std::string Hash(std::string const& password) { return "pbkdf2$TEST-ONLY$" + password; }
bool Verify(std::string const& password, std::string const& stored) {
    return stored == Hash(password) || stored == "legacy-test$" + password;
}
}
