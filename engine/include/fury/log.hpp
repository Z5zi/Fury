#pragma once

#include <string>

namespace fury {

enum class LogLevel { Trace, Info, Warn, Error };

class Log {
 public:
  static void set_level(LogLevel level);
  static LogLevel level();

  static void trace(const std::string& message);
  static void info(const std::string& message);
  static void warn(const std::string& message);
  static void error(const std::string& message);

 private:
  static void write(LogLevel level, const std::string& message);
  static LogLevel s_level;
};

}  // namespace fury
