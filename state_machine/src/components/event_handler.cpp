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

#include <set>

#include "logger.h"

namespace smf {

EventHandler::EventHandler(IStateManager* state_manager, IConditionManager* condition_manager,
                           ITransitionManager* transition_manager,
                           std::shared_ptr<StateEventHandler> state_event_handler)
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
  state_manager_->RegisterStateTimeoutCallback(std::bind(
      &EventHandler::TriggerStateTimeoutEvent, this, std::placeholders::_1, std::placeholders::_2));
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
  std::vector<TransitionRuleSharedPtr> rules;
  // 清理过期的待触发状态转移
  transition_manager_->RemoveExpiredPendingTransitions();
  // 首先检查待触发状态转移（优先级更高）
  if (transition_manager_->FindPendingTransition(current_state, event, rules)) {
    for (const auto& rule : rules) {
      std::vector<ConditionInfo> condition_infos;
      bool conditionsSatisfied = false;
      
      // 优先检查复杂条件表达式
      if (rule->HasConditionExprs()) {
        conditionsSatisfied = condition_manager_->CheckConditionExprs(
            rule->condition_exprs, condition_infos);
      } else {
        conditionsSatisfied = condition_manager_->CheckConditions(
            rule->conditions, rule->conditionsOperator, condition_infos);
      }
      
      if (conditionsSatisfied) {
        // 若挂起阶段已回调过 OnTransition，则消费时跳过 OnTransition，
        // 仅执行 OnExitState -> SetState -> OnEnterState
        const bool alreadyInvoked = transition_manager_->IsPendingTransitionInvoked(rule);
        ExecuteTransition(current_state, rule, event, condition_infos,
                          /*skip_on_transition=*/alreadyInvoked);
        eventHandled = true;
        transition_manager_->RemovePendingTransition(rule);
        break;  // 找到匹配的待触发转移后立即执行
      }
    }
    rules.clear();
  }

  // 如果没有处理待触发转移，则查找常规转换规则
  if (!eventHandled && transition_manager_->FindTransition(current_state, event, rules)) {
    for (const auto& rule : rules) {
      std::vector<ConditionInfo> condition_infos;
      bool conditionsSatisfied = false;
      
      // 优先检查复杂条件表达式
      if (rule->HasConditionExprs()) {
        conditionsSatisfied = condition_manager_->CheckConditionExprs(
            rule->condition_exprs, condition_infos);
      } else {
        conditionsSatisfied = condition_manager_->CheckConditions(
            rule->conditions, rule->conditionsOperator, condition_infos);
      }
      
      if (conditionsSatisfied) {
        // 执行状态转换，并传递满足的条件信息
        ExecuteTransition(current_state, rule, event, condition_infos);
        eventHandled = true;
        break;
      } else if (rule->timeout > 0) {
        // 如果条件不满足但配置了timeout，添加到待触发状态转移
        std::vector<ConditionInfo> unsatisfiedConditions;
        // 获取未满足的条件信息
        if (rule->HasConditionExprs()) {
          GetUnsatisfiedConditionsFromExprs(rule->condition_exprs, unsatisfiedConditions);
        } else {
          GetUnsatisfiedConditions(rule->conditions, rule->conditionsOperator, unsatisfiedConditions);
        }

        // 仅当真正新挂起（无重复）时，提前回调 OnTransition
        if (transition_manager_->AddPendingTransition(rule, event, unsatisfiedConditions)) {
          SMF_LOGI("Added pending transition for rule: " + rule->from + " -> " + rule->to +
                   " with timeout " + std::to_string(rule->timeout) + "ms");

          // 首次事件匹配但条件未满足：提前触发 OnTransition 回调
          if (state_event_handler_) {
            std::vector<State> exitStates;
            std::vector<State> enterStates;
            state_manager_->GetStateHierarchy(current_state, rule->to, exitStates, enterStates);
            SMF_LOGI("Pre-Transition (pending): " + current_state + " -> " + rule->to +
                     " on event " + event->toString() + ", waiting conditions");
            state_event_handler_->OnTransition(exitStates, event, enterStates);
          }
          transition_manager_->MarkPendingTransitionInvoked(rule);
        }
      }
    }
  }

  // 事件回收处理
  if (state_event_handler_) {
    state_event_handler_->OnPostEvent(event, eventHandled);
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
    
    // 检查条件是否满足：优先使用复杂条件表达式
    bool conditionsSatisfied = false;
    if (event_definition.HasConditionExprs()) {
      conditionsSatisfied = condition_manager_->CheckConditionExprs(
          event_definition.condition_exprs, condition_infos);
    } else {
      conditionsSatisfied = condition_manager_->CheckConditions(
          event_definition.conditions, event_definition.conditionsOperator, condition_infos);
    }
    
    if (conditionsSatisfied) {
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

void EventHandler::ExecuteTransition(const State& current_state,
                                     const TransitionRuleSharedPtr& rule, const EventPtr& event,
                                     const std::vector<ConditionInfo>& condition_infos,
                                     bool skip_on_transition) {
  // 获取状态层次结构
  std::vector<State> exitStates;
  std::vector<State> enterStates;
  state_manager_->GetStateHierarchy(current_state, rule->to, exitStates, enterStates);

  // 构建满足条件的信息字符串
  std::string conditionsStr;
  if (!condition_infos.empty()) {
    conditionsStr = " [";
    for (size_t i = 0; i < condition_infos.size(); ++i) {
      const auto& info = condition_infos[i];
      conditionsStr += info.name + "=" + std::to_string(info.value);
      if (info.duration > 0) {
        conditionsStr += " (sustain " + std::to_string(info.duration) + " ms)";
      }
      if (i < condition_infos.size() - 1) {
        conditionsStr += ", ";
      }
    }
    conditionsStr += "]";
  }

  // 执行状态转换
  if (state_event_handler_) {
    const std::string logPrefix = skip_on_transition ? "Transition (resume): " : "Transition: ";
    SMF_LOGI(logPrefix + current_state + " -> " + rule->to + " on event " + event->toString() +
             conditionsStr);
    if (!skip_on_transition) {
      state_event_handler_->OnTransition(exitStates, event, enterStates);
    }
    state_event_handler_->OnExitState(exitStates);
  }

  // 更新当前状态
  state_manager_->SetState(rule->to);

  // 调用状态进入处理
  if (state_event_handler_) {
    state_event_handler_->OnEnterState(enterStates);
  }
}

void EventHandler::GetUnsatisfiedConditions(const std::vector<ConditionSharedPtr>& conditions,
                                            const std::string& op,
                                            std::vector<ConditionInfo>& unsatisfiedConditions) {
  (void)op;
  unsatisfiedConditions.clear();

  for (const auto& cond : conditions) {
    int value = 0;
    condition_manager_->GetConditionValue(cond->name, value);

    if (!cond->IsValueInRange(value)) {
      unsatisfiedConditions.push_back({cond->name, value, 0});
    }
  }
}

void EventHandler::GetUnsatisfiedConditionsFromExprs(
    const std::vector<ConditionExprSharedPtr>& condition_exprs,
    std::vector<ConditionInfo>& unsatisfiedConditions) {
  unsatisfiedConditions.clear();

  // 收集所有条件表达式中的唯一条件名称
  std::set<std::string> conditionNames;
  for (const auto& expr : condition_exprs) {
    for (const auto& ref : expr->conditions) {
      conditionNames.insert(ref.name);
    }
  }

  // 遍历所有已定义的条件，检查其是否在表达式中被引用
  // 对于每个引用的条件，需要检查其是否真正满足
  // 由于复杂表达式的判断逻辑较复杂，这里简化处理：
  // 收集所有在表达式中的条件名，并记录其当前值
  for (const auto& name : conditionNames) {
    int value = 0;
    condition_manager_->GetConditionValue(name, value);
    // 注意：这里只记录条件的当前值，实际是否满足取决于表达式的完整计算
    // 由于表达式可能包含 AND/OR/取反逻辑，无法简单判断单个条件是否"不满足"
    // 这里记录所有条件信息供后续处理使用
    unsatisfiedConditions.push_back({name, value, 0});
  }
}

}  // namespace smf