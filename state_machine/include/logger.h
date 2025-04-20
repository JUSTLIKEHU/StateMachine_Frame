/**
 * @file logger.h
 * @brief Logger class definition
 * @author xiaokui.hu
 * @date 2025-04-05
 * @details This file contains the definition of the Logger class, which provides logging
 *          functionality for the state machine. The logger supports different log levels (DEBUG, INFO, WARN,
 *          ERROR) and can be configured to display timestamps with milliseconds. The logger is thread-safe
 *          and can be used in a multi-threaded environment.
 **/

/*
 * MIT License
 *
 * Copyright (c) 2025 xiaokui.hu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <chrono>  // 添加对chrono的引用
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace smf {

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
 public:
  static Logger& GetInstance() {
    static Logger instance;
    return instance;
  }

  void SetLogLevel(LogLevel level) { level_ = level; }

  LogLevel GetLogLevel() const { return level_; }

  void Log(LogLevel level, const std::string& file, int line, const std::string& message) {
    if (level < level_) {
      return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 使用chrono获取当前时间，包括毫秒
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&now_c);

    // 获取毫秒部分
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

    // 提取文件名，不显示完整路径
    std::string fileName = ExtractFileName(file);

    std::cerr << "[" << std::setw(2) << std::setfill('0') << localTime->tm_hour << ":"
              << std::setw(2) << std::setfill('0') << localTime->tm_min << ":" << std::setw(2)
              << std::setfill('0') << localTime->tm_sec << "." << std::setw(3) << std::setfill('0')
              << millis << "] "  // 添加毫秒显示
              << LevelToString(level) << " "
              << "[" << fileName << ":" << line << " - " << std::this_thread::get_id() << "] "
              << message << std::endl;
  }

 private:
  Logger() : level_(LogLevel::INFO) {}
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  std::string LevelToString(LogLevel level) const {
    switch (level) {
      case LogLevel::DEBUG:
        return "[DEBUG]";
      case LogLevel::INFO:
        return "[INFO] ";
      case LogLevel::WARN:
        return "[WARN] ";
      case LogLevel::ERROR:
        return "[ERROR]";
      default:
        return "[UNKNOWN]";
    }
  }

  // 辅助函数：从完整路径中提取文件名
  std::string ExtractFileName(const std::string& fullPath) const {
    size_t pos = fullPath.find_last_of("/\\");
    if (pos == std::string::npos)
      return fullPath;
    return fullPath.substr(pos + 1);
  }

  LogLevel level_;
  std::mutex mutex_;
};

}  // namespace smf

// Initialization macro
#define SMF_LOGGER_INIT(level) smf::Logger::GetInstance().SetLogLevel(level)

// Log macros for users
#define SMF_LOGD(message) \
  smf::Logger::GetInstance().Log(smf::LogLevel::DEBUG, __FILE__, __LINE__, message)
#define SMF_LOGI(message) \
  smf::Logger::GetInstance().Log(smf::LogLevel::INFO, __FILE__, __LINE__, message)
#define SMF_LOGW(message) \
  smf::Logger::GetInstance().Log(smf::LogLevel::WARN, __FILE__, __LINE__, message)
#define SMF_LOGE(message) \
  smf::Logger::GetInstance().Log(smf::LogLevel::ERROR, __FILE__, __LINE__, message)
