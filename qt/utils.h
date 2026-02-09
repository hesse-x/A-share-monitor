#ifndef UTILS_H
#define UTILS_H
#include <stdexcept>
#include <string_view>
#include <vector>

std::vector<std::string_view> splitString(std::string_view s, char delimiter);

inline static bool isStock(std::string_view code) {
  return (code.starts_with("sh") || code.starts_with("sz"));
}

inline static bool isFuture(std::string_view code) {
  return code.starts_with("IH") || code.starts_with("IF") ||
         code.starts_with("IC") || code.starts_with("IM");
}

void checkCode(std::string_view code);

namespace std {
class unset_env : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
} // namespace std
template <typename T> T getenv(std::string_view name);
#endif // UITLS_H
