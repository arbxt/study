#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

Logger &Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::set_level(LogLevel level) { level_ = level; }

void Logger::log(LogLevel level, const std::string &message) {
  // 过滤日志
  if (level < level_) {
    return;
  }

  // 获取当前时间
  auto now = std::chrono::system_clock::now();

  auto time = std::chrono::system_clock::to_time_t(now);

  std::tm tm_now{};

  localtime_r(&time, &tm_now);

  // 时间
  std::cout << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");

  // 日志等级
  switch (level) {
  case LogLevel::DEBUG:
    std::cout << " [DEBUG] ";
    break;

  case LogLevel::INFO:
    std::cout << " [INFO ] ";
    break;

  case LogLevel::WARN:
    std::cout << " [WARN ] ";
    break;

  case LogLevel::ERROR:
    std::cout << " [ERROR] ";
    break;
  }

  // 日志内容
  std::cout << message << std::endl;
}