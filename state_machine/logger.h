#pragma once

#include <iostream>
#include <string>
#include <ctime>
#include <chrono>  // 添加对chrono的引用
#include <iomanip>
#include <mutex>
#include <thread>

namespace smf {

enum class LogLevel {
  DEBUG,
  INFO,
  WARN,
  ERROR
};

class Logger {
public:
  static Logger& getInstance() {
    static Logger instance;
    return instance;
  }

  void setLogLevel(LogLevel level) {
    m_level = level;
  }

  LogLevel getLogLevel() const {
    return m_level;
  }

  void log(LogLevel level, const std::string& file, int line, const std::string& message) {
    if (level < m_level) {
      return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 使用chrono获取当前时间，包括毫秒
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&now_c);
    
    // 获取毫秒部分
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

    // 提取文件名，不显示完整路径
    std::string fileName = extractFileName(file);

    std::cerr << "["
          << std::setw(2) << std::setfill('0') << localTime->tm_hour << ":"
          << std::setw(2) << std::setfill('0') << localTime->tm_min << ":"
          << std::setw(2) << std::setfill('0') << localTime->tm_sec << "."
          << std::setw(3) << std::setfill('0') << millis << "] "  // 添加毫秒显示
          << levelToString(level) << " "
          << "["
          << fileName << ":" << line << " - "
          << std::this_thread::get_id() << "] "
          << message << std::endl;
  }

private:
  Logger() : m_level(LogLevel::INFO) {}
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  std::string levelToString(LogLevel level) const {
    switch (level) {
      case LogLevel::DEBUG: return "[DEBUG]";
      case LogLevel::INFO:  return "[INFO] ";
      case LogLevel::WARN:  return "[WARN] ";
      case LogLevel::ERROR: return "[ERROR]";
      default:              return "[UNKNOWN]";
    }
  }

  // 辅助函数：从完整路径中提取文件名
  std::string extractFileName(const std::string& fullPath) const {
    size_t pos = fullPath.find_last_of("/\\");
    if (pos == std::string::npos)
      return fullPath;
    return fullPath.substr(pos + 1);
  }

  LogLevel m_level;
  std::mutex m_mutex;
};

} // namespace smf

// Initialization macro
#define SMF_LOGGER_INIT(level) smf::Logger::getInstance().setLogLevel(level)

// Log macros for users
#define SMF_LOGD(message) smf::Logger::getInstance().log(smf::LogLevel::DEBUG, __FILE__, __LINE__, message)
#define SMF_LOGI(message) smf::Logger::getInstance().log(smf::LogLevel::INFO, __FILE__, __LINE__, message)
#define SMF_LOGW(message) smf::Logger::getInstance().log(smf::LogLevel::WARN, __FILE__, __LINE__, message)
#define SMF_LOGE(message) smf::Logger::getInstance().log(smf::LogLevel::ERROR, __FILE__, __LINE__, message)
