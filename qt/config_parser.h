#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <vector>

class ConfigData {
public:
  ConfigData() : freq(60000), codes({"sh000001"}) {}
  int64_t freq;
  std::vector<std::string> codes;
};

std::optional<ConfigData> parseConfig(std::istream &ins);
#endif // CONFIG_PARSER_H
