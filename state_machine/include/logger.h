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

#include <atomic>
#include <chrono>  // 添加对chrono的引用
#include <condition_variable>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace smf {

// 添加ANSI颜色码常量
namespace Color {
const std::string RESET = "\033[0m";
const std::string WHITE = "\033[37m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string RED = "\033[31m";
}  // namespace Color

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
 public:
  static Logger& GetInstance() {
    static Logger instance;
    return instance;
  }

  void SetLogLevel(LogLevel level) { level_ = level; }

  void SetLogFile(const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_file_ = file;
    if (ofs_.is_open())
      ofs_.close();
    if (!log_file_.empty())
      ofs_.open(log_file_, std::ios::app);
  }

  // 新增：设置循环日志参数
  void SetLogFileRolling(size_t max_file_size, int max_backup_index) {
    max_log_file_size_ = max_file_size;
    max_backup_index_ = max_backup_index;
  }

  LogLevel GetLogLevel() const { return level_; }

  void Log(LogLevel level, const std::string& file, int line, const std::string& message) {
    if (level < level_) {
      return;
    }

    // 格式化日志字符串
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&now_c);
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;
    std::string fileName = ExtractFileName(file);

    std::ostringstream oss;
    oss << "[" << std::setw(2) << std::setfill('0') << localTime->tm_hour << ":" << std::setw(2)
        << std::setfill('0') << localTime->tm_min << ":" << std::setw(2) << std::setfill('0')
        << localTime->tm_sec << "." << std::setw(3) << std::setfill('0') << millis << "] "
        << LevelToString(level) << " "
        << "[" << fileName << ":" << line << " - " << std::this_thread::get_id() << "] " << message
        << Color::RESET;

    std::string logStr = oss.str();

    // 控制台输出
    {
      std::lock_guard<std::mutex> lock(mutex_);
      std::cerr << logStr << std::endl;
    }
    // 推入日志队列，后台线程写盘
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      log_queue_.push(logStr);
      queue_cv_.notify_one();
    }
  }

  void Shutdown() {
    running_ = false;
    queue_cv_.notify_one();
    if (bg_thread_.joinable())
      bg_thread_.join();
    if (ofs_.is_open())
      ofs_.close();
  }

 private:
  Logger()
      : level_(LogLevel::INFO),
        log_file_(""),
        running_(true),
        bg_thread_(&Logger::BackgroundWrite, this),
        max_log_file_size_(10 * 1024 * 1024),  // 默认10MB
        max_backup_index_(3) {}

  ~Logger() { Shutdown(); }

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  std::string LevelToString(LogLevel level) const {
    switch (level) {
      case LogLevel::DEBUG:
        return Color::WHITE + "[DEBUG]";
      case LogLevel::INFO:
        return Color::GREEN + "[INFO] ";
      case LogLevel::WARN:
        return Color::YELLOW + "[WARN] ";
      case LogLevel::ERROR:
        return Color::RED + "[ERROR]";
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

  void BackgroundWrite() {
    while (running_ || !log_queue_.empty()) {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait_for(lock, std::chrono::milliseconds(100),
                         [this] { return !log_queue_.empty() || !running_; });

      while (!log_queue_.empty()) {
        std::string logStr = log_queue_.front();
        log_queue_.pop();
        lock.unlock();
        if (!log_file_.empty()) {
          std::lock_guard<std::mutex> file_lock(mutex_);
          RotateIfNeeded();
          if (!ofs_.is_open()) {
            ofs_.open(log_file_, std::ios::app);
          }
          if (ofs_.is_open()) {
            ofs_ << logStr << std::endl;
            ofs_.flush();
          }
        }
        lock.lock();
      }
    }
  }

  void RotateIfNeeded() {
    if (log_file_.empty() || max_log_file_size_ == 0)
      return;
    ofs_.seekp(0, std::ios::end);
    std::streampos size = ofs_.tellp();
    if (size < 0)
      return;
    if (static_cast<size_t>(size) < max_log_file_size_)
      return;

    ofs_.close();
    // 删除最老的备份
    if (max_backup_index_ > 0) {
      std::string oldest = log_file_ + "." + std::to_string(max_backup_index_);
      std::remove(oldest.c_str());
      // 依次重命名
      for (int i = max_backup_index_ - 1; i >= 1; --i) {
        std::string src = log_file_ + "." + std::to_string(i);
        std::string dst = log_file_ + "." + std::to_string(i + 1);
        std::rename(src.c_str(), dst.c_str());
      }
      // 当前日志文件重命名为 .1
      std::rename(log_file_.c_str(), (log_file_ + ".1").c_str());
    } else {
      std::remove(log_file_.c_str());
    }
    ofs_.open(log_file_, std::ios::trunc);
  }

  LogLevel level_;
  std::string log_file_;
  std::ofstream ofs_;
  std::mutex mutex_;
  std::queue<std::string> log_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::atomic<bool> running_;
  std::thread bg_thread_;
  // 新增循环日志参数
  size_t max_log_file_size_;
  int max_backup_index_;
};

}  // namespace smf

// Initialization macro
#define SMF_LOGGER_INIT(level) smf::Logger::GetInstance().SetLogLevel(level)

#define SMF_LOGGER_SET_FILE(file) smf::Logger::GetInstance().SetLogFile(file)
// Set log file rolling
#define SMF_LOGGER_SET_ROLLING(max_file_size, max_backup_index) \
  smf::Logger::GetInstance().SetLogFileRolling(max_file_size, max_backup_index)

// Log macros for users
#define SMF_LOGD(message) \
  smf::Logger::GetInstance().Log(smf::LogLevel::DEBUG, __FILE__, __LINE__, message)
#define SMF_LOGI(message) \
  smf::Logger::GetInstance().Log(smf::LogLevel::INFO, __FILE__, __LINE__, message)
#define SMF_LOGW(message) \
  smf::Logger::GetInstance().Log(smf::LogLevel::WARN, __FILE__, __LINE__, message)
#define SMF_LOGE(message) \
  smf::Logger::GetInstance().Log(smf::LogLevel::ERROR, __FILE__, __LINE__, message)
