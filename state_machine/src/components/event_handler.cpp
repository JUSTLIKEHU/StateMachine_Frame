/**
 * @file event_handler.cpp
 * @brief Implementation of event handler component
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file contains the implementation of the EventHandler class, which
 *          processes events in the state machine and determines if they trigger
 *          state transitions based on the current state and event conditions.
 * @version 1.0.0
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

#include "components/event_handler.h"

#include "logger.h"

namespace smf {

EventHandler::EventHandler(IStateManager* state_manager, IConditionManager* condition_manager,
                           ITransitionManager* transition_manager,
                           StateEventHandler* state_event_handler)
    : state_manager_(state_manager),
      condition_manager_(condition_manager),
      transition_manager_(transition_manager),
      state_event_handler_(state_event_handler) {
  if (state_manager_ == nullptr || condition_manager_ == nullptr ||
      transition_manager_ == nullptr || state_event_handler_ == nullptr) {
    SMF_LOGE("Invalid parameters");
    return;
  }
  condition_manager_->RegisterConditionChangeCallback(
      std::bind(&EventHandler::TriggerEvent, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
  state_manager_->RegisterStateTimeoutCallback(
      std::bind(&EventHandler::TriggerStateTimeoutEvent, this, std::placeholders::_1,
                std::placeholders::_2));
}

EventHandler::~EventHandler() { Stop(); }

void EventHandler::Start() {
  if (running_) {
    return;
  }
  running_ = true;
  event_thread_ = std::thread(&EventHandler::EventLoop, this);
}

void EventHandler::Stop() {
  if (!running_) {
    return;
  }
  running_ = false;
  event_cv_.notify_all();
  if (event_thread_.joinable()) {
    event_thread_.join();
  }
}

bool EventHandler::IsRunning() const { return running_; }

void EventHandler::HandleEvent(const EventPtr& event) {
  std::lock_guard<std::mutex> lock(event_mutex_);
  event_queue_.push(event);
  event_cv_.notify_one();
}

bool EventHandler::AddEventDefinition(const EventDefinition& event_definition) {
  if (running_) {
    SMF_LOGE("EventHandler is running, cannot add event definition");
    return false;
  }
  event_definitions_.emplace_back(event_definition);
  return true;
}

void EventHandler::ProcessEvent(const EventPtr& event) {
  State current_state = state_manager_->GetCurrentState();

  // 事件预处理
  if (state_event_handler_ && !state_event_handler_->OnPreEvent(current_state, event)) {
    if (state_event_handler_) {
      state_event_handler_->OnPostEvent(event, false);
    }
    return;
  }

  bool eventHandled = false;
  std::vector<TransitionRule> rules;

  // 查找可用的转换规则
  if (transition_manager_->FindTransition(current_state, event, rules)) {
    for (const auto& rule : rules) {
      std::vector<ConditionInfo> condition_infos;
      if (condition_manager_->CheckConditions(rule.conditions, rule.conditionsOperator,
                                              condition_infos)) {
        PrintSatisfiedConditions(condition_infos);
        // 获取状态层次结构
        std::vector<State> exitStates;
        std::vector<State> enterStates;
        state_manager_->GetStateHierarchy(current_state, rule.to, exitStates, enterStates);

        // 执行状态转换
        if (state_event_handler_) {
          SMF_LOGI("Transition: " + current_state + " -> " + rule.to + " on event " +
                   event->toString());
          state_event_handler_->OnTransition(exitStates, event, enterStates);
          state_event_handler_->OnExitState(exitStates);
        }

        // 更新当前状态
        state_manager_->SetState(rule.to);

        // 调用状态进入处理
        if (state_event_handler_) {
          state_event_handler_->OnEnterState(enterStates);
        }
        eventHandled = true;
      }
    }

    // 事件回收处理
    if (state_event_handler_) {
      state_event_handler_->OnPostEvent(event, eventHandled);
    }
  }
}

void EventHandler::EventLoop() {
  while (running_) {
    EventPtr event{nullptr};
    {
      std::unique_lock<std::mutex> lock(event_mutex_);
      event_cv_.wait(lock, [this] { return !running_ || !event_queue_.empty(); });

      if (!running_) {
        break;
      }

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

void EventHandler::TriggerEvent(const std::string& condition_name, int value, int duration,
                                bool value_in_range) {
  SMF_LOGD("TriggerEvent: " + condition_name + " " + std::to_string(value) + " " +
           std::to_string(value_in_range));
  for (const auto& event_definition : event_definitions_) {
    std::vector<ConditionInfo> condition_infos;
    int event_condition_value = 0;
    condition_manager_->GetConditionValue(event_definition.name, event_condition_value);
    if (condition_manager_->CheckConditions(event_definition.conditions,
                                            event_definition.conditionsOperator, condition_infos)) {
      // 如果条件满足，且对应事件条件当前值为0，边缘触发与水平触发均可触发事件
      if (event_condition_value == 0) {
        // 更新事件同名条件值为1
        condition_manager_->SetConditionValue(event_definition.name, 1);
        EventPtr eventPtr = std::make_shared<Event>(event_definition.name);
        eventPtr->SetMatchedConditions(condition_infos);
        HandleEvent(eventPtr);
      } else {
        // 如果条件满足，且对应事件条件当前值为1，水平触发可触发事件
        if (event_definition.trigger_mode == "level") {
          EventPtr eventPtr = std::make_shared<Event>(event_definition.name);
          eventPtr->SetMatchedConditions(condition_infos);
          HandleEvent(eventPtr);
        }
      }
    } else {
      // 如果条件不满足，且对应事件条件当前值为1，边缘触发 event_definition.name + "_RESET" 事件
      if (event_condition_value == 1) {
        // 将事件同名条件值重置为0
        condition_manager_->SetConditionValue(event_definition.name, 0);
        if (event_definition.trigger_mode == "edge") {
          EventPtr eventPtr = std::make_shared<Event>(event_definition.name + "_RESET");
          HandleEvent(eventPtr);
        }
      }
    }
  }

  // 所有条件更新都支持触发内部事件
  EventPtr eventPtr = std::make_shared<Event>(INTERNAL_EVENT);
  std::vector<ConditionInfo> condition_infos;
  condition_infos.emplace_back(ConditionInfo{condition_name, value, duration});
  eventPtr->SetMatchedConditions(condition_infos);
  HandleEvent(eventPtr);
}

void EventHandler::TriggerStateTimeoutEvent(const State& state, int timeout) {
  SMF_LOGD("TriggerStateTimeoutEvent: " + state + " " + std::to_string(timeout));
  EventPtr eventPtr = std::make_shared<Event>(STATE_TIMEOUT_EVENT);
  HandleEvent(eventPtr);
}

void EventHandler::PrintSatisfiedConditions(
    const std::vector<ConditionInfo>& condition_infos) const {
  for (const auto& condition_info : condition_infos) {
    SMF_LOGD(std::string("SatisfiedCondition: condition_name: ") + condition_info.name + ", " +
             "condition_value: " + std::to_string(condition_info.value) + ", " +
             "condition_duration: " + std::to_string(condition_info.duration));
  }
}

}  // namespace smf