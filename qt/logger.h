#ifndef LOGGER_H
#define LOGGER_H
#include <filesystem> // IWYU pragma: keep
#include <sstream>

// 日志级别枚举
enum class LogLevel { DEBUG, INFO, WARNING, ERROR, FATAL };

class Logger : public std::ostringstream {
private:
  LogLevel level_; // log level

public:
  explicit Logger(LogLevel level);
  ~Logger() override;
};

#define LOG(level)                                                             \
  Logger(LogLevel::level)                                                      \
      << std::filesystem::path(__FILE__).filename().c_str() << "(" << __LINE__ \
      << "):"
#endif // LOGGER_H
