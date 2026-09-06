#include "fury/log.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>

namespace fury {

LogLevel Log::s_level = LogLevel::Info;

void Log::set_level(LogLevel level) { s_level = level; }
LogLevel Log::level() { return s_level; }

void Log::trace(const std::string& message) { write(LogLevel::Trace, message); }
void Log::info(const std::string& message) { write(LogLevel::Info, message); }
void Log::warn(const std::string& message) { write(LogLevel::Warn, message); }
void Log::error(const std::string& message) { write(LogLevel::Error, message); }

void Log::write(LogLevel lvl, const std::string& message) {
  if (static_cast<int>(lvl) < static_cast<int>(s_level)) {
    return;
  }

  const char* tag = "INFO";
  switch (lvl) {
    case LogLevel::Trace: tag = "TRACE"; break;
    case LogLevel::Info:  tag = "INFO"; break;
    case LogLevel::Warn:  tag = "WARN"; break;
    case LogLevel::Error: tag = "ERROR"; break;
  }

  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const std::time_t t = clock::to_time_t(now);
  std::tm tm_buf{};
#if defined(_WIN32)
  localtime_s(&tm_buf, &t);
#else
  localtime_r(&t, &tm_buf);
#endif
  char time_str[16];
  std::snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", tm_buf.tm_hour,
                tm_buf.tm_min, tm_buf.tm_sec);

  std::ostream& out = (lvl == LogLevel::Error || lvl == LogLevel::Warn)
                          ? std::cerr
                          : std::cout;
  out << '[' << time_str << "][Fury][" << tag << "] " << message << '\n';
}

}  // namespace fury
