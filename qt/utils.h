#ifndef UTILS_H
#define UTILS_H
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

#endif // UITLS_H
