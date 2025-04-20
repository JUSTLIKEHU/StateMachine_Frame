/**
 *@file   state_machine.cpp
 *@brief  State machine implementation
 *@author xiaokui.hu
 *@date   2025-04-13
 *@details This file contains the implementation of the FiniteStateMachine class, which implements a
 *         finite state machine.
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

#include "state_machine.h"
#include <algorithm>
#include <mutex>
#include "logger.h"

namespace smf {

bool FiniteStateMachine::Init(const std::string& configFile) {
  try {
    LoadFromJSON(configFile);
    initialized_ = true;
    return true;
  } catch (const std::exception& e) {
    SMF_LOGE("Initialization failed: " + std::string(e.what()));
    return false;
  }
}

bool FiniteStateMachine::Start() {
  if (!initialized_) {
    SMF_LOGE("State machine not initialized!");
    return false;
  }

  if (running_) {
    SMF_LOGW("State machine already running!");
    return false;
  }

  running_ = true;
  event_thread_ = std::thread(&FiniteStateMachine::EventLoop, this);
  event_trigger_thread_ = std::thread(&FiniteStateMachine::EventTriggerLoop, this);
  condition_thread_ = std::thread(&FiniteStateMachine::ConditionLoop, this);
  timer_thread_ = std::thread(&FiniteStateMachine::TimerLoop, this);
  return true;
}

void FiniteStateMachine::Stop() {
  running_ = false;
  // 通知所有等待中的线程
  {
    std::lock_guard<std::mutex> lock(event_mutex_);
    event_cv_.notify_one();
  }
  {
    std::lock_guard<std::mutex> lock_condition(condition_update_mutex_);
    condition_update_cv_.notify_one();
  }
  {
    std::lock_guard<std::mutex> lock_timer(timer_mutex_);
    timer_cv_.notify_one();
  }
  {
    std::lock_guard<std::mutex> lock_event_trigger(event_trigger_mutex_);
    event_trigger_cv_.notify_one();
  }

  // 等待线程结束
  if (timer_thread_.joinable()) {
    timer_thread_.join();
  }
  if (condition_thread_.joinable()) {
    condition_thread_.join();
  }
  if (event_thread_.joinable()) {
    event_thread_.join();
  }
  if (event_trigger_thread_.joinable()) {
    event_trigger_thread_.join();
  }
}

void FiniteStateMachine::HandleEvent(const Event& event) {
  std::lock_guard<std::mutex> lock(event_mutex_);
  event_queue_.push(event);
  event_cv_.notify_one();
}

void FiniteStateMachine::SetTransitionCallback(StateEventHandler::TransitionCallback callback) {
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetTransitionCallback(std::move(callback));
}

void FiniteStateMachine::SetPreEventCallback(StateEventHandler::PreEventCallback callback) {
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetPreEventCallback(std::move(callback));
}

void FiniteStateMachine::SetEnterStateCallback(StateEventHandler::EnterStateCallback callback) {
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetEnterStateCallback(std::move(callback));
}

void FiniteStateMachine::SetExitStateCallback(StateEventHandler::ExitStateCallback callback) {
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetExitStateCallback(std::move(callback));
}

void FiniteStateMachine::SetPostEventCallback(StateEventHandler::PostEventCallback callback) {
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetPostEventCallback(std::move(callback));
}

void FiniteStateMachine::SetStateEventHandler(std::shared_ptr<StateEventHandler> handler) {
  state_event_handler_ = handler;
}

void FiniteStateMachine::AddState(const State& name, const State& parent) {
  if (states_.find(name) != states_.end()) {
    throw std::invalid_argument("State already exists: " + name);
  }

  states_[name] = {name, parent, {}};
  if (!parent.empty()) {
    states_[parent].children.push_back(name);
  }
}

void FiniteStateMachine::AddTransition(const TransitionRule& rule) {
  if (states_.find(rule.from) == states_.end() || states_.find(rule.to) == states_.end()) {
    throw std::invalid_argument("State does not exist");
  }

  // 如果事件为空，则使用内部事件
  if (rule.event.empty()) {
    TransitionRule ruleWithInternalEvent = rule;
    ruleWithInternalEvent.event = Event(INTERNAL_EVENT);
    event_transitions_.insert({{rule.from, ruleWithInternalEvent.event}, ruleWithInternalEvent});
  } else {
    event_transitions_.insert({{rule.from, rule.event}, rule});
  }
}

void FiniteStateMachine::SetInitialState(const State& state) {
  if (states_.find(state) == states_.end()) {
    throw std::invalid_argument("Initial state does not exist");
  }
  current_state_ = state;
}

State FiniteStateMachine::GetCurrentState() const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return current_state_;
}

void FiniteStateMachine::SetConditionValue(const std::string& name, int value) {
  std::lock_guard<std::mutex> lock(condition_update_mutex_);
  ConditionUpdateEvent update{name, value, std::chrono::steady_clock::now()};
  condition_update_queue_.push(update);
  condition_update_cv_.notify_one();  // 通知条件处理线程
}

void FiniteStateMachine::LoadFromJSON(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open JSON file");
  }

  json config;
  file >> config;

  // 加载状态
  for (const auto& state : config["states"]) {
    State name = state["name"];
    State parent = state.value("parent", "");
    AddState(name, parent);
  }

  // 加载初始状态
  SetInitialState(config["initial_state"]);

  // 加载事件定义（新增）
  if (config.contains("events")) {
    for (const auto& eventJson : config["events"]) {
      EventDefinition eventDef;
      eventDef.name = eventJson["name"];
      eventDef.trigger_mode = eventJson.value("trigger_mode", "edge");  // 默认为边缘触发
      eventDef.conditionsOperator = eventJson.value("conditions_operator", "AND");  // 默认为AND条件

      // 加载条件
      if (eventJson.contains("conditions")) {
        for (const auto& condition : eventJson["conditions"]) {
          Condition cond;
          cond.name = condition["name"];
          cond.range = {condition["range"][0], condition["range"][1]};
          cond.duration = condition.value("duration", 0);  // 默认为0
          eventDef.conditions.push_back(cond);
          all_conditions_.push_back(cond);
        }
      }
      // 添加到事件定义列表
      event_definitions_.push_back(eventDef);
    }
  }

  // 加载状态转移规则
  for (const auto& transition : config["transitions"]) {
    TransitionRule rule;
    rule.from = transition["from"];
    rule.event = transition.value("event", "");  // 事件可为空
    rule.to = transition["to"];
    rule.conditionsOperator = transition.value("conditions_operator", "AND");

    // 加载条件
    if (transition.contains("conditions")) {
      for (const auto& condition : transition["conditions"]) {
        Condition cond;
        cond.name = condition["name"];
        cond.range = {condition["range"][0], condition["range"][1]};
        cond.duration = condition.value("duration", 0);  // 默认为0
        rule.conditions.push_back(cond);
        all_conditions_.push_back(cond);
      }
    }
    AddTransition(rule);
  }

  // 初始化条件的默认值为 0
  for (const auto& cond : all_conditions_) {
    if (condition_values_.find(cond.name) == condition_values_.end()) {
      condition_values_[cond.name] = {cond.name, 0, std::chrono::steady_clock::now(),
                                      std::chrono::steady_clock::now(), false};
    }
  }
}

void FiniteStateMachine::EventLoop() {
  while (running_) {
    Event event;
    {
      std::unique_lock<std::mutex> lock(event_mutex_);
      event_cv_.wait(lock, [this] { return !running_ || !event_queue_.empty(); });

      if (!running_)
        break;

      if (!event_queue_.empty()) {
        event = event_queue_.front();
        event_queue_.pop();
      } else {
        continue;
      }
    }
    // 处理事件
    ProcessEvent(event);
  }
}

void FiniteStateMachine::ConditionLoop() {
  while (running_) {
    static std::queue<ConditionUpdateEvent> conditionQueueCopy;
    {
      std::unique_lock<std::mutex> lock(condition_update_mutex_);
      condition_update_cv_.wait(lock,
                                [this] { return !running_ || !condition_update_queue_.empty(); });
      if (!running_)
        break;
      if (!condition_update_queue_.empty()) {
        // SMF_LOGD("Before condition copy queue size: " + std::to_string(conditionQueueCopy.size())
        // +
        //          ", condition queue size: " + std::to_string(condition_update_queue_.size()));
        condition_update_queue_.swap(conditionQueueCopy);  // 交换队列
        // SMF_LOGD("After condition copy queue size: " + std::to_string(conditionQueueCopy.size())
        // +
        //          ", condition queue size: " + std::to_string(condition_update_queue_.size()));
      }
    }
    ProcessConditionUpdates(conditionQueueCopy);
  }
}

std::vector<State> FiniteStateMachine::GetStateHierarchy(const State& state) const {
  std::vector<State> hierarchy;
  State current = state;

  while (!current.empty()) {
    hierarchy.emplace_back(current);
    auto it = states_.find(current);
    if (it == states_.end())
      break;
    current = it->second.parent;
  }

  return hierarchy;
}

void FiniteStateMachine::GetStateHierarchy(const State& from, const State& to,
                                         std::vector<State>& exit_states,
                                         std::vector<State>& enter_states) const {
  // 获取起始状态的层次结构
  auto fromStates = GetStateHierarchy(from);
  // 获取目标状态的层次结构
  auto toStates = GetStateHierarchy(to);

  // 找到共同的父状态
  auto itFrom = fromStates.rbegin();
  auto itTo = toStates.rbegin();
  while (itFrom != fromStates.rend() && itTo != toStates.rend() && *itFrom == *itTo) {
    ++itFrom;
    ++itTo;
  }

  // 添加需要退出的状态
  for (; itFrom != fromStates.rend(); ++itFrom) {
    exit_states.emplace_back(*itFrom);
  }

  std::reverse(exit_states.begin(), exit_states.end());
  
  // 添加需要进入的状态
  for (; itTo != toStates.rend(); ++itTo) {
    enter_states.emplace_back(*itTo);
  }
}

void FiniteStateMachine::ProcessEvent(const Event& event) {
  bool eventHandled = false;
  {
    State state = current_state_;

    // 事件预处理
    if (state_event_handler_ && !state_event_handler_->OnPreEvent(current_state_, event)) {
      // 事件被拒绝处理
      if (state_event_handler_) {
        state_event_handler_->OnPostEvent(event, false);
      }
      return;
    }

    while (!state.empty()) {
      auto key = std::make_pair(state, event);
      // 查找当前状态和事件的所有转移规则
      auto range = event_transitions_.equal_range(key);
      for (auto it = range.first; it != range.second; ++it) {
        const auto& rule = it->second;
        if (CheckConditions(rule.conditions, rule.conditionsOperator)) {
          // 获取起始状态和目标状态的完整层次结构
          std::vector<State> exitStates;
          std::vector<State> enterStates;
          GetStateHierarchy(state, rule.to, exitStates, enterStates);
          // 执行状态转换
          if (state_event_handler_) {
            state_event_handler_->OnTransition(exitStates, event, enterStates);
          }
          // 调用状态退出处理
          if (state_event_handler_) {
            state_event_handler_->OnExitState(exitStates);
          }
          {
            std::lock_guard<std::mutex> lock(state_mutex_);
            current_state_ = rule.to;
          }
          // 调用状态进入处理
          if (state_event_handler_) {
            state_event_handler_->OnEnterState(enterStates);
          }

          SMF_LOGI("Transition: " + state + " -> " + current_state_ + " on event " + event.GetName());
          PrintSatisfiedConditions(rule.conditions);

          eventHandled = true;
          break;
        }
      }
      if (eventHandled) {
        break;  // 找到匹配的转移规则，退出循环
      }
      state = states_[state].parent;
    }
  }
  // 事件回收处理
  if (state_event_handler_) {
    state_event_handler_->OnPostEvent(event, eventHandled);
  }
}

void FiniteStateMachine::CheckConditionTransitions() {
  // 此方法不再使用，所有状态转换都通过事件处理
}

void FiniteStateMachine::PrintSatisfiedConditions(const std::vector<Condition>& conditions) {
  std::lock_guard<std::mutex> lock(condition_values_mutex_);
  if (!conditions.empty()) {
    SMF_LOGD("Satisfied conditions:");
    for (const auto& cond : conditions) {
      int value = condition_values_[cond.name].value;
      if (value >= cond.range.first && value <= cond.range.second) {
        SMF_LOGD("  - " + cond.name + " = " + std::to_string(value) + " (range: [" +
                 std::to_string(cond.range.first) + ", " + std::to_string(cond.range.second) +
                 "],duration: " + std::to_string(cond.duration) + ")");
      }
    }
  }
}

bool FiniteStateMachine::CheckConditions(const std::vector<Condition>& conditions,
                                         const std::string& op) {
  std::lock_guard<std::mutex> lock(condition_values_mutex_);
  if (conditions.empty()) {
    return true;  // 无条件限制
  }

  auto now = std::chrono::steady_clock::now();

  for (const auto& cond : conditions) {
    if (condition_values_.find(cond.name) == condition_values_.end()) {
      throw std::invalid_argument("Condition value not set: " + cond.name);
    }

    int value = condition_values_[cond.name].value;
    bool valueInRange = (value >= cond.range.first && value <= cond.range.second);

    // 检查持续时间
    if (cond.duration > 0 && valueInRange) {
      // 如果条件配置了持续时间，检查当前时间是否超过持续时间
      // 检查是否在已触发的持续条件中
      if (condition_values_[cond.name].isTriggered) {
        // 此持续条件已经满足并触发过，无需再次等待
        if (op == "OR") {
          return true;  // OR 条件满足
        }
        continue;  // 继续检查下一个条件
      }

      // 否则需要检查计时器
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - condition_values_[cond.name].lastChangedTime)
                         .count();
      valueInRange = (elapsed >= cond.duration);

      if (valueInRange) {
        // 如果时间条件满足，记录此条件已触发
        condition_values_[cond.name].isTriggered = true;
      }
    }

    if (op == "AND" && !valueInRange) {
      return false;  // AND 条件不满足
    } else if (op == "OR" && valueInRange) {
      return true;  // OR 条件满足
    }
  }

  return (op == "AND");  // AND 条件全部满足，或 OR 条件全部不满足
}

void FiniteStateMachine::ProcessConditionUpdates(
    std::queue<ConditionUpdateEvent>& conditionUpdateQueue) {
  std::lock_guard<std::mutex> lock(condition_values_mutex_);
  while (!conditionUpdateQueue.empty()) {
    const auto& update = conditionUpdateQueue.front();
    int oldValue = condition_values_[update.name].value;
    condition_values_[update.name].value = update.value;

    // 只有当值改变时才进行后续处理
    if (oldValue != update.value) {
      condition_values_[update.name].lastChangedTime = update.updateTime;
      condition_values_[update.name].isTriggered = false;  // 重置触发状态

      bool hasDurationCondition = false;

      // 检查此条件值是否配置了持续时间
      for (const auto& cond : all_conditions_) {
        if (cond.name == update.name) {
          if (cond.duration > 0 &&
              (update.value >= cond.range.first && update.value <= cond.range.second)) {
            // 如果条件配置了持续时间且当前值在范围内，添加到定时器
            std::lock_guard<std::mutex> timerLock(timer_mutex_);
            auto expiryTime = update.updateTime + std::chrono::milliseconds(cond.duration);
            timer_queue_.push({update.name, update.value, cond.duration, expiryTime});
            timer_cv_.notify_one();
            hasDurationCondition = true;
            break;  // 一个条件名只添加一个定时器
          }
        }
      }

      // 如果没有持续时间条件，需要触发事件生成检查
      if (!hasDurationCondition) {
        event_trigger_cv_.notify_one();
        SMF_LOGD("Condition updated: " + update.name + " to " + std::to_string(update.value));
      }
    }
    conditionUpdateQueue.pop();
  }
}

void FiniteStateMachine::TriggerEvent() {
  // 检查所有事件定义
  for (const auto& eventDef : event_definitions_) {
    bool conditionsMet = CheckConditions(eventDef.conditions, eventDef.conditionsOperator);
    {
      std::lock_guard<std::mutex> lock(condition_values_mutex_);
      int currentEventConditionValue = condition_values_[eventDef.name].value;
      // 检查事件条件是否满足
      if (conditionsMet) {
        // 如果条件满足，且对应事件条件当前值为0（边缘触发）
        if (currentEventConditionValue == 0) {
          // 更新事件同名条件值为1
          condition_values_[eventDef.name].value = 1;
          condition_values_[eventDef.name].lastUpdateTime = std::chrono::steady_clock::now();
          condition_values_[eventDef.name].lastChangedTime = std::chrono::steady_clock::now();

          SMF_LOGI("Event condition met: " + eventDef.name +
                   ", triggered event and set condition to 1");

          // 触发事件
          if (eventDef.trigger_mode == "edge") {
            // 创建事件并包含所有相关条件值
            Event newEvent(eventDef.name);
            for (const auto& cond : eventDef.conditions) {
              // 使用统一的SetCondition接口，同时处理条件值和持续时间
              newEvent.SetCondition(cond.name, condition_values_[cond.name].value,
                                    cond.duration > 0 ? cond.duration : 0);
            }
            HandleEvent(newEvent);
          }
        }
      } else {
        // 条件不满足，且对应事件条件当前值为1（表示之前条件满足过）
        if (currentEventConditionValue == 1) {
          // 将事件同名条件值重置为0
          condition_values_[eventDef.name].value = 0;
          condition_values_[eventDef.name].lastUpdateTime = std::chrono::steady_clock::now();
          condition_values_[eventDef.name].lastChangedTime = std::chrono::steady_clock::now();

          SMF_LOGI("Event condition no longer met: " + eventDef.name + ", reset condition to 0");

          // 如果是边缘触发模式，在条件消失时也触发一次事件
          if (eventDef.trigger_mode == "edge") {
            // 这里可以选择触发特定的事件，例如 eventDef.name + "_RESET"
            // 或者直接触发内部事件进行状态检查
            HandleEvent(Event(INTERNAL_EVENT));
          }
        }
      }
    }
  }
  // 所有条件更新都支持触发内部事件
  HandleEvent(Event(INTERNAL_EVENT));
}

void FiniteStateMachine::TimerLoop() {
  while (running_) {
    std::unique_lock<std::mutex> lock(timer_mutex_);
    if (timer_queue_.empty()) {
      timer_cv_.wait(lock, [this] { return !running_ || !timer_queue_.empty(); });
      continue;
    }
    auto now = std::chrono::steady_clock::now();
    if (now >= timer_queue_.top().expiryTime) {
      auto expiredCondition = timer_queue_.top();
      timer_queue_.pop();
      SMF_LOGD("Duration condition expired: " + expiredCondition.name + " with value " +
               std::to_string(expiredCondition.value));
      // 检查当前条件值是否仍然匹配触发时的值并检查检查状态持续时间是否满足
      {
        std::lock_guard<std::mutex> condLock(condition_values_mutex_);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - condition_values_[expiredCondition.name].lastChangedTime)
                           .count();
        if (condition_values_.find(expiredCondition.name) != condition_values_.end() &&
            condition_values_[expiredCondition.name].value == expiredCondition.value &&
            elapsed >= expiredCondition.duration) {
          // 记录此条件已触发
          condition_values_[expiredCondition.name].isTriggered = true;
          SMF_LOGI("Duration condition triggered: " + expiredCondition.name + " with value " +
                   std::to_string(expiredCondition.value));
          // 触发事件检查
          std::lock_guard<std::mutex> eventLock(event_trigger_mutex_);
          event_trigger_cv_.notify_one();
        }
      }
    } else {
      timer_cv_.wait_until(lock, timer_queue_.top().expiryTime);
    }
  }
}

void FiniteStateMachine::EventTriggerLoop() {
  while (running_) {
    std::unique_lock<std::mutex> lock(event_trigger_mutex_);
    event_trigger_cv_.wait(lock);
    if (!running_)
      break;
    // 触发事件
    TriggerEvent();
  }
}

}  // namespace smf