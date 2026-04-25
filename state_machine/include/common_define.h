/**
 * @file common_define.h
 * @brief Common definitions for the state machine framework.
 * @details This file contains common definitions, including type aliases and constants,
 *          used throughout the state machine framework.
 * @author xiaokui.hu
 * @date 2025-04-13
 * @version 1.0
 * @note This file is part of the state machine framework and should be included in
 *       any source file that uses the framework.
 *       It provides a central location for common definitions to avoid code duplication.
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

#include <chrono>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace smf {

// 前向声明
class Event;
using EventPtr = std::shared_ptr<Event>;

// 定义内部事件
inline constexpr const char* INTERNAL_EVENT = "__INTERNAL_EVENT__";
inline constexpr const char* STATE_TIMEOUT_EVENT = "__STATE_TIMEOUT_EVENT__";

// 定义状态的类型
using State = std::string;

// 条件类型
struct Condition {
  std::string name;                               // 条件名称
  std::vector<std::pair<int, int>> range_values;  // 条件范围数组 [[min1, max1], [min2, max2], ...]
  int duration{0};  // 条件持续时间(毫秒),默认0表示立即生效

  bool operator==(const Condition& other) const noexcept {
    return name == other.name && range_values == other.range_values && duration == other.duration;
  }

  bool operator!=(const Condition& other) const noexcept { return !(*this == other); }

  // 检查值是否在任何范围内
  bool IsValueInRange(int value) const noexcept {
    for (const auto& range : range_values) {
      if (value >= range.first && value <= range.second) {
        return true;
      }
    }
    return false;
  }
};

using ConditionSharedPtr = std::shared_ptr<Condition>;

// 条件引用结构体，用于复杂条件表达式
struct ConditionRef {
  std::string name;      // 条件名称
  bool negated{false};   // 是否取反（!A 表示 A 条件不满足）

  bool operator==(const ConditionRef& other) const noexcept {
    return name == other.name && negated == other.negated;
  }

  bool operator!=(const ConditionRef& other) const noexcept { return !(*this == other); }
};

// 条件表达式结构体，表示一个条件组合
// 例如: "A AND B OR C" 表示为:
//   conditions = [A, B, C]
//   operators = ["AND", "OR"]
// 按顺序从左到右依次计算: (A AND B) OR C
struct ConditionExpr {
  std::vector<ConditionRef> conditions;    // 条件引用列表
  std::vector<std::string> operators;      // 操作符列表 (AND/OR)，数量 = conditions.size() - 1

  bool IsValid() const noexcept {
    if (conditions.empty()) return false;
    return operators.size() == conditions.size() - 1;
  }

  bool operator==(const ConditionExpr& other) const noexcept {
    return conditions == other.conditions && operators == other.operators;
  }

  bool operator!=(const ConditionExpr& other) const noexcept { return !(*this == other); }
};

using ConditionExprSharedPtr = std::shared_ptr<ConditionExpr>;

struct ConditionValue {
  std::string name;                                       // 条件名称
  int value;                                              // 条件值
  std::chrono::steady_clock::time_point lastUpdateTime;   // 最后一次更新时间
  std::chrono::steady_clock::time_point lastChangedTime;  // 上次变化时间
};

// 条件信息
struct ConditionInfo {
  std::string name;  // 条件名称
  int value;         // 条件值
  long duration;     // 持续时间，单位为毫秒

  bool operator==(const ConditionInfo& other) const noexcept {
    return name == other.name && value == other.value && duration == other.duration;
  }

  bool operator!=(const ConditionInfo& other) const noexcept { return !(*this == other); }
};

// 状态转移规则
struct TransitionRule {
  State from;                                          // 起始状态
  std::vector<std::string> events;                     // 事件列表（可为空）
  State to;                                            // 目标状态
  std::vector<ConditionSharedPtr> conditions;          // 条件列表（简单模式）
  std::string conditionsOperator;                      // 条件运算符 ("AND" 或 "OR")（简单模式）
  std::vector<ConditionExprSharedPtr> condition_exprs; // 复杂条件表达式列表，满足任意一个即可
  int timeout{0};  // 状态转移超时时间(毫秒)，默认0表示不超时

  // 检查是否有条件（简单模式或复杂表达式模式）
  bool HasConditions() const noexcept { 
    return !conditions.empty() || !condition_exprs.empty(); 
  }
  
  // 检查是否使用复杂条件表达式模式
  bool HasConditionExprs() const noexcept { return !condition_exprs.empty(); }
  
  bool HasTimeout() const noexcept { return timeout > 0; }
  bool IsEventTriggered(const std::string& eventName) const noexcept {
    for (const auto& e : events) {
      if (e == eventName) return true;
    }
    return false;
  }
};

using TransitionRuleSharedPtr = std::shared_ptr<TransitionRule>;

// 状态信息
struct StateInfo {
  State name;                   // 状态名称
  State parent;                 // 父状态名称（可为空）
  std::vector<State> children;  // 子状态列表
  int timeout{0};               // 状态超时时间(毫秒)，默认0表示不超时

  bool HasParent() const noexcept { return !parent.empty(); }
  bool HasChildren() const noexcept { return !children.empty(); }
  bool HasTimeout() const noexcept { return timeout > 0; }
};

// 状态超时信息
struct StateTimeoutInfo {
  State state;
  int timeout;
  std::chrono::steady_clock::time_point enterTime;
  std::chrono::steady_clock::time_point expiryTime;
};

// 在类定义之前添加条件更新事件结构体
struct ConditionUpdateEvent {
  std::string name;
  int value;
  std::chrono::steady_clock::time_point updateTime;
};

// 在ConditionUpdateEvent结构体后添加定时条件结构体
struct DurationCondition {
  std::string name;
  int value;     // 添加值字段，用于跟踪触发条件时的值
  int duration;  // 记录持续时间，单位为毫秒，用于定时器是否满足
  std::chrono::steady_clock::time_point expiryTime;
};

// 添加事件定义结构体
struct EventDefinition {
  std::string name;          // 事件名称
  std::string trigger_mode;  // 触发模式：edge (边缘触发) 或 level (水平触发)
  std::vector<ConditionSharedPtr> conditions;          // 触发事件的条件列表（简单模式）
  std::string conditionsOperator;                      // 条件运算符 ("AND" 或 "OR")（简单模式）
  std::vector<ConditionExprSharedPtr> condition_exprs; // 复杂条件表达式列表，满足任意一个即可
  
  // 检查是否使用复杂条件表达式模式
  bool HasConditionExprs() const noexcept { return !condition_exprs.empty(); }
};

// 添加待触发状态转移结构体
struct PendingTransition {
  TransitionRuleSharedPtr rule;                      // 转移规则
  std::vector<std::string> triggerEvents;            // 触发事件
  std::chrono::steady_clock::time_point createTime;  // 创建时间
  std::chrono::steady_clock::time_point expiryTime;  // 超时时间
  std::vector<ConditionInfo> unsatisfiedConditions;  // 未满足的条件信息
  bool onTransitionInvoked{false};                   // OnTransition 回调是否已在挂起阶段触发过
  EventPtr originalEvent;  // 触发挂起时的用户事件（resume 时回调统一使用该事件，
                           // 避免回调中出现内部事件造成困惑）
};

}  // namespace smf
