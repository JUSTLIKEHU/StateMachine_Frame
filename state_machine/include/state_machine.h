/**
 * @file state_machine.h
 * @brief State machine class definition
 * @author xiaokui.hu
 * @date 2025-01-26
 * @details This file contains the definition of the FiniteStateMachine class, which implements a
 *          finite state machine. The state machine supports state transitions based on events and conditions,
 *          and allows for nested states. The state machine can be configured using a JSON file, and supports
 *          callbacks for state transitions, event handling, and condition updates. The state machine is
 *          thread-safe and can be used in a multi-threaded environment.
 **/

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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common_define.h"
#include "logger.h"                // 添加日志头文件
#include "nlohmann-json/json.hpp"  // 引入 nlohmann/json 库
#include "state_event_handler.h"   // 引入状态事件处理器头文件

namespace smf {

using json = nlohmann::json;

// 有限状态机类
class FiniteStateMachine {
 public:
  // 定义内部事件常量
  static constexpr const char* INTERNAL_EVENT = "__INTERNAL_EVENT__";  // 使用更明确的名称

  FiniteStateMachine() : running_(false), initialized_(false) {}
  ~FiniteStateMachine() {
    // 合并了停止定时器和状态机的逻辑
    Stop();
  }

  // 初始化接口
  bool Init(const std::string& configFile);

  // 启动状态机
  bool Start();

  // 停止状态机
  void Stop();

  // 处理事件（线程安全）
  void HandleEvent(const Event& event);

  // 设置状态转移回调 - 函数对象版本
  void SetTransitionCallback(StateEventHandler::TransitionCallback callback);

  // 设置状态转移回调 - 类成员函数版本
  template <typename T>
  void SetTransitionCallback(T* instance, void (T::*method)(const std::vector<State>&, const Event&,
                                                            const std::vector<State>&)) {
    if (!state_event_handler_) {
      state_event_handler_ = std::make_shared<StateEventHandler>();
    }
    state_event_handler_->SetTransitionCallback(instance, method);
  }

  // 设置事件预处理回调 - 函数对象版本
  void SetPreEventCallback(StateEventHandler::PreEventCallback callback);

  // 设置事件预处理回调 - 类成员函数版本
  template <typename T>
  void SetPreEventCallback(T* instance, bool (T::*method)(const State&, const Event&)) {
    if (!state_event_handler_) {
      state_event_handler_ = std::make_shared<StateEventHandler>();
    }
    state_event_handler_->SetPreEventCallback(instance, method);
  }

  // 设置状态进入回调 - 函数对象版本
  void SetEnterStateCallback(StateEventHandler::EnterStateCallback callback);

  // 设置状态进入回调 - 类成员函数版本
  template <typename T>
  void SetEnterStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    if (!state_event_handler_) {
      state_event_handler_ = std::make_shared<StateEventHandler>();
    }
    state_event_handler_->SetEnterStateCallback(instance, method);
  }

  // 设置状态退出回调 - 函数对象版本
  void SetExitStateCallback(StateEventHandler::ExitStateCallback callback);

  // 设置状态退出回调 - 类成员函数版本
  template <typename T>
  void SetExitStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    if (!state_event_handler_) {
      state_event_handler_ = std::make_shared<StateEventHandler>();
    }
    state_event_handler_->SetExitStateCallback(instance, method);
  }

  // 设置事件回收回调 - 函数对象版本
  void SetPostEventCallback(StateEventHandler::PostEventCallback callback);

  // 设置事件回收回调 - 类成员函数版本
  template <typename T>
  void SetPostEventCallback(T* instance, void (T::*method)(const Event&, bool)) {
    if (!state_event_handler_) {
      state_event_handler_ = std::make_shared<StateEventHandler>();
    }
    state_event_handler_->SetPostEventCallback(instance, method);
  }

  // 继续支持原有的接口，但现在作为兼容层
  void SetStateEventHandler(std::shared_ptr<StateEventHandler> handler);

  // 添加状态
  void AddState(const State& name, const State& parent = "");

  // 添加状态转移规则
  void AddTransition(const TransitionRule& rule);

  // 设置初始状态
  void SetInitialState(const State& state);

  // 获取当前状态
  State GetCurrentState() const;

  // 修改设置条件值接口为异步处理
  void SetConditionValue(const std::string& name, int value);

  // 从 JSON 文件加载状态机配置
  void LoadFromJSON(const std::string& filepath);

  // 获取状态及其所有父状态（从子到父的顺序）
  std::vector<State> GetStateHierarchy(const State& state) const;
  void GetStateHierarchy(const State& from, const State& to, std::vector<State>& exit_states,
                         std::vector<State>& enter_states) const;

 private:
  // 事件处理循环 - 专注于处理事件队列
  void EventLoop();

  // 条件处理循环 - 专注于处理条件更新
  void ConditionLoop();

  // 处理单个事件
  void ProcessEvent(const Event& event);

  // 检查条件触发规则 - 这个方法不再使用
  // 保留代码但标记为废弃，供参考
  void CheckConditionTransitions();

  // 新增：打印满足的条件
  void PrintSatisfiedConditions(const std::vector<Condition>& conditions);

  // 检查条件是否满足
  bool CheckConditions(const std::vector<Condition>& conditions, const std::string& op);

  // 处理条件更新队列
  void ProcessConditionUpdates(std::queue<ConditionUpdateEvent>& conditionUpdateQueue);

  // 添加新方法：检查事件条件
  void TriggerEvent();

  // 新增定时器循环处理
  void TimerLoop();

  // 事件触发循环
  void EventTriggerLoop();

  // 存储所有状态
  std::unordered_map<State, StateInfo> states_;

  // 存储事件触发的状态转移规则 - 修改为multimap允许同一状态下存在多个同名事件的转移规则
  std::multimap<std::pair<State, Event>, TransitionRule> event_transitions_;

  // 当前状态
  State current_state_;

  // 条件值
  std::unordered_map<std::string, ConditionValue> condition_values_;
  std::mutex condition_values_mutex_;

  // 状态转移处理器
  std::shared_ptr<StateEventHandler> state_event_handler_;

  // 新增成员变量
  std::atomic_bool running_{false};  // 运行状态
  bool initialized_{false};  // 是否已初始化
  std::thread event_thread_;
  std::thread event_trigger_thread_;
  std::thread condition_thread_;
  std::thread timer_thread_;
  std::queue<Event> event_queue_;
  std::mutex event_mutex_;
  std::condition_variable event_cv_;
  std::mutex event_trigger_mutex_;
  std::condition_variable event_trigger_cv_;
  std::queue<ConditionUpdateEvent> condition_update_queue_;
  std::mutex condition_update_mutex_;
  std::condition_variable condition_update_cv_;
  mutable std::mutex state_mutex_;
  // 添加新的成员变量来存储所有条件
  std::vector<Condition> all_conditions_;
  std::priority_queue<DurationCondition, std::vector<DurationCondition>,
                      std::function<bool(const DurationCondition&, const DurationCondition&)>>
      timer_queue_{[](const DurationCondition& lhs, const DurationCondition& rhs) {
        return lhs.expiryTime > rhs.expiryTime;
      }};
  std::mutex timer_mutex_;
  std::condition_variable timer_cv_;
  // 在类的成员变量部分添加事件定义存储
  std::vector<EventDefinition> event_definitions_;
};

}  // namespace smf
