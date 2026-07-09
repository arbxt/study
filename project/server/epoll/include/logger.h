#pragma once

#include <string>

enum class LogLevel { DEBUG = 0, INFO, WARN, ERROR };

class Logger {
public:
  static Logger &instance();

  void set_level(LogLevel level);

  void log(LogLevel level, const std::string &message);

private:
  Logger() = default;

  LogLevel level_ = LogLevel::DEBUG;
};

#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::DEBUG, msg)

#define LOG_INFO(msg) Logger::instance().log(LogLevel::INFO, msg)

#define LOG_WARN(msg) Logger::instance().log(LogLevel::WARN, msg)

#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, msg)