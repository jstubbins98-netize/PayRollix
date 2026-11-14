#pragma once
#include <string>
namespace security {
std::string Hash(std::string const& password);
bool Verify(std::string const& password, std::string const& hash);
}
