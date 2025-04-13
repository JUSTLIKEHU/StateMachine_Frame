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

#include <string>
#include <chrono>
#include <vector>
namespace smf {
// 定义状态和事件的类型
using State = std::string;
using Event = std::string;

// 条件类型
struct Condition {
  std::string name;           // 条件名称
  std::pair<int, int> range;  // 条件范围 [min, max]
  int duration{0};            // 条件持续时间(毫秒),默认0表示立即生效
  std::chrono::steady_clock::time_point lastUpdateTime;  // 最后一次更新时间
};

// 状态转移规则
struct TransitionRule {
  State from;                         // 起始状态
  Event event;                        // 事件（可为空）
  State to;                           // 目标状态
  std::vector<Condition> conditions;  // 条件列表
  std::string conditionsOperator;     // 条件运算符 ("AND" 或 "OR")
};

// 状态信息
struct StateInfo {
  State name;                   // 状态名称
  State parent;                 // 父状态名称（可为空）
  std::vector<State> children;  // 子状态列表
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
  int value;  // 添加值字段，用于跟踪触发条件时的值
  std::chrono::steady_clock::time_point expiryTime;
};

// 添加事件定义结构体
struct EventDefinition {
  std::string name;                   // 事件名称
  std::string trigger_mode;           // 触发模式：edge (边缘触发) 或 level (水平触发)
  std::vector<Condition> conditions;  // 触发事件的条件列表
  std::string conditionsOperator;     // 条件运算符 ("AND" 或 "OR")
};
}  // namespace smf