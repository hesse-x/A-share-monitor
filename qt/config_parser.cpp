#include <algorithm>
#include <cstddef>
#include <string_view>

#include "config_parser.h"
#include "logger.h"
static bool isComment(std::string_view str) {
  return str.size() > 0 && str.front() == '#';
}

// Trims whitespace characters from both the start and end of a string view.
static std::string_view trim(std::string_view sv) {
  // Define common whitespace characters: space, tab, newline, carriage return,
  // form feed, vertical tab
  constexpr const char *whitespace = " \t\n\r\f\v";

  // Find the position of the first non-whitespace character
  size_t start = sv.find_first_not_of(whitespace);
  if (start == std::string_view::npos) {
    return {}; // Entire string is whitespace, return empty view
  }

  // Find the position of the last non-whitespace character
  size_t end = sv.find_last_not_of(whitespace);

  // Adjust the view to the trimmed range [start, end] (inclusive of end
  // character)
  return sv.substr(start, end - start + 1);
}

static int64_t parseTime(std::string_view time_sv) {
  // Find first non-digit character using find_first_not_of (more efficient)
  size_t unit_pos = time_sv.find_first_not_of("0123456789");

  // Must have both digits and unit
  if (unit_pos == std::string_view::npos || unit_pos == 0)
    return -1; // No unit (all digits) or no digits (starts with non-digit)

  // Parse numeric part
  int64_t num;
  try {
    num = std::stoll(std::string(time_sv.substr(0, unit_pos)));
  } catch (...) {
    return -1; // Invalid number or overflow
  }

  if (num < 0) {
    return -1; // Negative values not allowed
  }

  // Parse unit and convert to milliseconds
  std::string_view unit = time_sv.substr(unit_pos);
  if (unit.empty() || unit == "ms") {
    return num;
  } else if (unit == "s") {
    return num * 1000;
  } else if (unit == "m") {
    return num * 60 * 1000;
  } else {
    return -1; // Unknown unit
  }
}

static void removeDuplicates(std::vector<std::string> &vec) {
  std::sort(vec.begin(), vec.end());
  auto last = std::unique(vec.begin(), vec.end());
  vec.erase(last, vec.end());
}

std::optional<ConfigData> parseConfig(std::istream &ins) {
  ConfigData result;
  result.codes.clear();
  enum class State {
    INIT,      // Wait "code:" or "freq:"
    READ_CODE, // Read content after "code:"
    READ_FREQ  // Read content after "freq:"
  } state = State::INIT;

  std::string line;
  int line_num = 0;

  while (std::getline(ins, line)) {
    line_num++;
    std::string_view line_sv = line;
    std::string_view trimmed = trim(line_sv);

    // Skip empty line.
    if (trimmed.empty() || isComment(trimmed)) {
      continue;
    }

    switch (state) {
    case State::INIT:
      if (trimmed == "code:") {
        state = State::READ_CODE;
      } else if (trimmed == "freq:") {
        state = State::READ_FREQ;
      } else {
        LOG(ERROR) << "Parse config failed(line: " << line_num
                   << "), unexpected content, expected 'code:' or 'freq:'";
        return std::nullopt;
      }
      break;

    case State::READ_CODE:
      if (trimmed == "freq:") {
        state = State::READ_FREQ;
      } else {
        LOG(INFO) << "parse code: " << trimmed;
        result.codes.emplace_back(trimmed);
      }
      break;

    case State::READ_FREQ:
      LOG(INFO) << "parse freq: " << trimmed;
      int64_t time = parseTime(trimmed);
      if (time == -1) {
        LOG(ERROR) << "Parse config failed(line: " << line_num
                   << "), invalid time format (e.g., '1ms' or '1s' or '1m')";
        return std::nullopt;
      }
      result.freq = time;
      state = State::INIT;
      break;
    }
  }

  // Check incompleted state.
  if (state == State::READ_FREQ) {
    LOG(ERROR) << "Parse config failed(line: " << line_num
               << "), unexpected end of input while reading freq value";
    return std::nullopt;
  }
  if (state == State::READ_CODE && result.codes.empty()) {
    LOG(ERROR) << "Parse config failed(line: " << line_num
               << "), unexpected end of input while reading code value";
    return std::nullopt;
  }

  removeDuplicates(result.codes);
  return result;
}
