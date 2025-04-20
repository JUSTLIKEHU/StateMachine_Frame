/**
 * @file Event.h
 * @brief Event definition
 * @date 2025-04-13
 * @details This file contains the definition of the Event class, which represents an event in the
 *          state machine. The Event class includes event name, conditions, and other properties. It
 *          also includes methods for setting and getting condition values, and for printing event
 *          information.
 * @author xiaokui.hu
 * @version 1.0
 * @note This file is part of the state machine framework and should be included in
 *       any source file that uses the framework.
 *       It provides a central location for event definitions to avoid code duplication.
 *       This file is designed to be used with the FiniteStateMachine class and the
 *       StateEventHandler class.
 */

/**
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

#include <map>
#include <ostream>
#include <string>

#include "logger.h"

namespace smf {

// 保存持续条件的结构：值和持续时间(毫秒)
struct DurationInfo {
  int value;
  int durationMs;
};

class Event {
 public:
  // 默认构造函数
  Event() = default;

  // 从事件名称构造，提供向后兼容性
  Event(const std::string& name) : name_(name) {}
  Event(const char* name) : name_(name) {}

  // 构造函数，包含事件名称和条件值
  Event(const std::string& name, const std::map<std::string, DurationInfo>& conditionValues)
      : name_(name), durationConditions_(conditionValues) {}

  // 获取事件名称
  const std::string& GetName() const { return name_; }

  // 获取条件值
  int GetConditionValue(const std::string& conditionName) const {
    auto it = durationConditions_.find(conditionName);
    return it != durationConditions_.end() ? it->second.value : 0;
  }

  void SetCondition(const std::string& conditionName, int value, int durationMs = 0) {
    if (conditionName.empty() || value < 0) {
      SMF_LOGE("conditionName is empty or value is less than 0");
    }
    durationConditions_[conditionName] = {value, durationMs};
  }

  void Clear() { durationConditions_.clear(); }

  // 将事件转换为字符串（隐式转换）
  operator std::string() const { return name_; }

  // 重载比较运算符，通过事件名称比较
  bool operator==(const Event& other) const { return name_ == other.name_; }
  bool operator!=(const Event& other) const { return name_ != other.name_; }
  bool operator<(const Event& other) const { return name_ < other.name_; }
  bool operator>(const Event& other) const { return name_ > other.name_; }

  // 与字符串比较的特殊重载
  bool operator==(const std::string& name) const { return name_ == name; }
  bool operator!=(const std::string& name) const { return name_ != name; }

  // 与字符串字面量比较的特殊重载，避免歧义
  bool operator==(const char* name) const { return name_ == name; }
  bool operator!=(const char* name) const { return name_ != name; }

  // 判断事件是否为空
  bool empty() const { return name_.empty(); }

  // 添加字符串转换函数，用于日志输出等场景
  std::string toString() const {
    std::string result = name_;

    if (!durationConditions_.empty()) {
      result += " [";
      bool first = true;
      for (const auto& pair : durationConditions_) {
        if (!first)
          result += ", ";
        result += pair.first + "=" + std::to_string(pair.second.value);
        if (pair.second.durationMs > 0) {
          result += " (sustain " + std::to_string(pair.second.durationMs) + " ms)";
        }
        first = false;
      }

      result += "]";
    }
    return result;
  }

 private:
  std::string name_;                                        // 事件名称
  std::map<std::string, DurationInfo> durationConditions_;  // 保存条件的值

  // 友元，用于实现流输出运算符
  friend std::ostream& operator<<(std::ostream& os, const Event& event);
};

// 重载流输出运算符
inline std::ostream& operator<<(std::ostream& os, const Event& event) {
  os << event.name_;
  if (!event.durationConditions_.empty()) {
    os << " [";
    bool first = true;
    for (const auto& pair : event.durationConditions_) {
      if (!first)
        os << ", ";
      os << pair.first << "=" << pair.second.value;
      if (pair.second.durationMs > 0) {
        os << " (sustain " << pair.second.durationMs << " ms)";  // 添加持续时间的输出
      }
      first = false;
    }

    os << "]";
  }

  return os;
}

}  // namespace smf
