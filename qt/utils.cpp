#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>

#include "utils.h"

std::vector<std::string_view> splitString(std::string_view s, char delimiter) {
  std::vector<std::string_view> tokens;
  size_t start = 0;
  size_t end = s.find(delimiter);

  while (end != std::string_view::npos) {
    tokens.emplace_back(s.substr(start, end - start));
    start = end + 1;
    end = s.find(delimiter, start);
  }

  if (start < s.size()) {
    tokens.emplace_back(s.substr(start));
  }

  return tokens;
}

std::optional<std::string> checkCode(std::string_view code) {
  if (isStock(code)) {
    if (code.size() != 8)
      return "Stock code length must be 8 characters (e.g., sh600000, "
             "sz000001)";
    code = code.substr(2);
    if (std::any_of(code.begin(), code.end(),
                    [](const char c) { return !std::isdigit(c); }))
      return "Last six characters must be digits (0-9)";
    return std::nullopt;
  }
  if (isFuture(code)) {
    if (code[2] != '-')
      return "Future code must be split name and type with - (e.g., IC-Front, "
             "IF-Next)";
    code = code.substr(3);
    if (!(code == "Front" || code == "Next"))
      return "Future code type only support Front/Next - (e.g., IH-Front, "
             "IM-Next)";
    return std::nullopt;
  }
  return "code must start with sh/sz(stock) or IH/IF/IC/IM(future)";
}
