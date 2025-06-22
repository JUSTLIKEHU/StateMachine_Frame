/**
 * @file transition_manager.cpp
 * @brief Implementation of transition manager component
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file contains the implementation of the TransitionManager class,
 *          which is responsible for storing and retrieving state transition rules
 *          that define when and how states can change.
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

#include "components/transition_manager.h"

#include <algorithm>

#include "common_define.h"
#include "logger.h"

namespace smf {

TransitionManager::TransitionManager() = default;

TransitionManager::~TransitionManager() { Stop(); }

void TransitionManager::Start() {
  bool expected = false;
  if (running_.compare_exchange_strong(expected, true)) {
    SMF_LOGI("TransitionManager started");
  }
}

void TransitionManager::Stop() {
  bool expected = true;
  if (running_.compare_exchange_strong(expected, false)) {
    SMF_LOGI("TransitionManager stopped");
  }
}

bool TransitionManager::IsRunning() const { return running_; }

bool TransitionManager::AddTransition(const TransitionRuleSharedPtr& rule) {
  if (running_) {
    SMF_LOGE("Cannot add transition while running.");
    return false;
  }

  // 为每个事件添加转换规则
  for (const auto& event : rule->events) {
    TransitionKey key{rule->from, event};
    {
      std::unique_lock<std::shared_mutex> lock(mutex_);
      transitions_.insert({key, rule});

      std::string log_msg =
          "Added transition rule: " + rule->from + " -> " + rule->to + " on event " + event;
      SMF_LOGI(log_msg);
    }
  }
  return true;
}

bool TransitionManager::FindTransition(const State& current_state, const EventPtr& event,
                                       std::vector<TransitionRuleSharedPtr>& out_rules) {
  if (!running_) {
    SMF_LOGE("TransitionManager is not running");
    return false;
  }

  TransitionKey key{current_state, event->GetName()};

  {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto range = transitions_.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
      out_rules.push_back(it->second);
    }
  }

  return !out_rules.empty();
}

void TransitionManager::Clear() {
  if (!running_) {
    SMF_LOGE("TransitionManager is not running");
    return;
  }

  {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    transitions_.clear();
    SMF_LOGI("Cleared all transition rules");
  }
}

bool TransitionManager::AddPendingTransition(
    const TransitionRuleSharedPtr& rule, const EventPtr& event,
    const std::vector<ConditionInfo>& unsatisfiedConditions) {
  if (!running_) {
    SMF_LOGE("TransitionManager is not running");
    return false;
  }

  if (rule->timeout <= 0) {
    SMF_LOGW("Cannot add pending transition without timeout configuration");
    return false;
  }

  auto now = std::chrono::steady_clock::now();
  auto expiryTime = now + std::chrono::milliseconds(rule->timeout);

  PendingTransition pendingTransition{
      rule, {event->GetName(), INTERNAL_EVENT}, now, expiryTime, unsatisfiedConditions};

  {
    std::unique_lock<std::shared_mutex> lock(pending_mutex_);
    pending_transitions_.push_back(std::move(pendingTransition));

    std::string log_msg = "Added pending transition: " + rule->from + " -> " + rule->to +
                          " on event " + event->GetName() + " with timeout " +
                          std::to_string(rule->timeout) + "ms";
    SMF_LOGI(log_msg);
  }

  return true;
}

bool TransitionManager::FindPendingTransition(const State& current_state, const EventPtr& event,
                                              std::vector<TransitionRuleSharedPtr>& out_rules) {
  if (!running_) {
    SMF_LOGE("TransitionManager is not running");
    return false;
  }

  {
    std::shared_lock<std::shared_mutex> lock(pending_mutex_);
    for (const auto& pending : pending_transitions_) {
      if (pending.rule->from == current_state) {
        // 检查事件是否匹配
        bool eventMatches = false;
        for (const auto& ruleEvent : pending.triggerEvents) {
          if (ruleEvent == event->GetName()) {
            eventMatches = true;
            break;
          }
        }

        if (eventMatches) {
          out_rules.push_back(pending.rule);
        }
      }
    }
  }

  return !out_rules.empty();
}

void TransitionManager::RemoveExpiredPendingTransitions() {
  if (!running_) {
    return;
  }

  auto now = std::chrono::steady_clock::now();

  {
    std::unique_lock<std::shared_mutex> lock(pending_mutex_);
    auto it = std::remove_if(
        pending_transitions_.begin(), pending_transitions_.end(),
        [now](const PendingTransition& pending) { return now >= pending.expiryTime; });

    if (it != pending_transitions_.end()) {
      size_t removedCount = std::distance(it, pending_transitions_.end());
      pending_transitions_.erase(it, pending_transitions_.end());
      SMF_LOGI("Removed " + std::to_string(removedCount) + " expired pending transitions");
    }
  }
}

void TransitionManager::RemovePendingTransition(const TransitionRuleSharedPtr& rule) {
  if (!running_) {
    return;
  }

  {
    std::unique_lock<std::shared_mutex> lock(pending_mutex_);
    pending_transitions_.erase(
        std::remove_if(pending_transitions_.begin(), pending_transitions_.end(),
                       [rule](const PendingTransition& pending) { return pending.rule == rule; }),
        pending_transitions_.end());
    SMF_LOGI("Removed pending transition: " + rule->from + " -> " + rule->to);
  }
}

void TransitionManager::ClearPendingTransitions() {
  if (!running_) {
    return;
  }

  {
    std::unique_lock<std::shared_mutex> lock(pending_mutex_);
    pending_transitions_.clear();
    SMF_LOGI("Cleared all pending transitions");
  }
}

}  // namespace smf