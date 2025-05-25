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

bool TransitionManager::AddTransition(const TransitionRule& rule) {
  if (running_) {
    SMF_LOGE("Cannot add transition while running.");
    return false;
  }

  // 为每个事件添加转换规则
  for (const auto& event : rule.events) {
    TransitionKey key{rule.from, event};
    {
      std::unique_lock<std::shared_mutex> lock(mutex_);
      transitions_.insert({key, rule});

      std::string log_msg =
          "Added transition rule: " + rule.from + " -> " + rule.to + " on event " + event;
      SMF_LOGI(log_msg);
    }
  }
  return true;
}

bool TransitionManager::FindTransition(const State& current_state, const EventPtr& event,
                                       std::vector<TransitionRule>& out_rules) {
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

}  // namespace smf