#include "logger.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

static inline void localtime(time_t *now, tm *timeinfo) {
#ifdef _WIN32
  localtime_s(timeinfo, now);
#else
  localtime_r(now, timeinfo);
#endif
}

static inline void getCurrentTime(char *buffer, size_t len) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime(&now, &timeinfo);
  strftime(buffer, len, "[%Y-%m-%d %H:%M:%S]", &timeinfo);
  return;
}

constexpr std::array<const char *, 5> levelStr = {
    "[DEBUG]", "[INFO]", "[WARNING]", "[ERROR]", "[FATAL]"};

Logger::Logger(LogLevel level) {
  char time[80];
  getCurrentTime(time, sizeof(time));
  *this << time << levelStr[static_cast<int>(level)];
}

Logger::~Logger() {
  if (level_ == LogLevel::INFO)
    fprintf(stdout, "%s\n", this->str().c_str());
  else
    fprintf(stderr, "%s\n", this->str().c_str());
  if (level_ == LogLevel::FATAL)
    std::abort();
}
