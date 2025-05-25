/**
 * @file condition_manager.cpp
 * @brief Implementation of condition manager component
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file contains the implementation of the ConditionManager class,
 *          which is responsible for tracking and evaluating conditions that
 *          affect state transitions in the state machine.
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

#include "components/condition_manager.h"

#include <algorithm>

#include "logger.h"

namespace smf {

ConditionManager::ConditionManager() = default;

ConditionManager::~ConditionManager() { Stop(); }

void ConditionManager::Start() {
  if (running_) {
    return;
  }
  running_ = true;
  condition_thread_ = std::thread(&ConditionManager::ConditionLoop, this);
  timer_thread_ = std::thread(&ConditionManager::TimerLoop, this);
}

void ConditionManager::Stop() {
  if (!running_) {
    return;
  }
  running_ = false;
  condition_update_cv_.notify_all();
  if (condition_thread_.joinable()) {
    condition_thread_.join();
  }
  timer_cv_.notify_all();
  if (timer_thread_.joinable()) {
    timer_thread_.join();
  }
}

bool ConditionManager::IsRunning() const { return running_; }

void ConditionManager::SetConditionValue(const std::string& name, int value) {
  {
    std::lock_guard<std::mutex> lock(condition_update_mutex_);
    condition_update_queue_.push({name, value, std::chrono::steady_clock::now()});
  }
  condition_update_cv_.notify_one();
}

bool ConditionManager::CheckConditions(const std::vector<ConditionSharedPtr>& conditions,
                                       const std::string& op,
                                       std::vector<ConditionInfo>& condition_infos) {
  std::unordered_map<std::string, ConditionValue> values_copy;
  {
    std::lock_guard<std::mutex> lock(condition_values_mutex_);
    values_copy = condition_values_;
  }

  if (conditions.empty()) {
    return true;
  }

  if (op != "AND" && op != "OR") {
    throw std::invalid_argument("Invalid operator: " + op);
  }

  condition_infos.clear();
  auto now = std::chrono::steady_clock::now();

  for (const auto& cond : conditions) {
    auto it = values_copy.find(cond->name);
    if (it == values_copy.end()) {
      throw std::invalid_argument("Condition value not set: " + cond->name);
    }

    int value = it->second.value;
    bool valueInRange = false;

    // 检查值是否在任何范围内
    for (const auto& range : cond->range_values) {
      if (value >= range.first && value <= range.second) {
        valueInRange = true;
        break;
      }
    }

    // 检查持续时间
    if (cond->duration > 0 && valueInRange) {
      auto elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.lastChangedTime)
              .count();
      valueInRange = (elapsed >= cond->duration);
      if (valueInRange) {
        condition_infos.push_back({cond->name, value, elapsed});
      }
    }

    if (op == "AND" && !valueInRange) {
      condition_infos.clear();
      return false;
    } else if (op == "OR" && valueInRange) {
      return true;
    }
  }

  return (op == "AND");
}

void ConditionManager::AddCondition(const ConditionSharedPtr& condition) {
  if (running_) {
    SMF_LOGE("Cannot add condition while running");
    return;
  }
  std::lock_guard<std::mutex> lock(condition_values_mutex_);
  all_conditions_.push_back(condition);

  // 初始化条件值
  if (condition_values_.find(condition->name) == condition_values_.end()) {
    auto now = std::chrono::steady_clock::now();
    condition_values_[condition->name] = {condition->name,
                                         0,  // 初始值
                                         now, now};
  }
}

void ConditionManager::GetConditionValue(const std::string& name, int& value) const {
  std::lock_guard<std::mutex> lock(condition_values_mutex_);
  auto it = condition_values_.find(name);
  if (it != condition_values_.end()) {
    value = it->second.value;
  } else {
    value = 0;
    SMF_LOGW("Condition value not set: " + name + ", return 0");
  }
}

void ConditionManager::RegisterConditionChangeCallback(ConditionChangeCallback callback) {
  if (running_) {
    SMF_LOGE("Cannot register condition change callback while running");
    return;
  }
  std::lock_guard<std::mutex> lock(callback_mutex_);
  condition_change_callback_ = std::move(callback);
}

void ConditionManager::ConditionLoop() {
  while (running_) {
    {
      std::unique_lock<std::mutex> lock(condition_update_mutex_);
      condition_update_cv_.wait(lock,
                                [this] { return !running_ || !condition_update_queue_.empty(); });

      if (!running_) {
        break;
      }
    }

    ProcessConditionUpdates();
  }
}

void ConditionManager::TimerLoop() {
  while (running_) {
    // 局部变量用于存储过期条件
    DurationCondition expiredCondition;
    bool hasExpiredCondition = false;
    std::chrono::steady_clock::time_point nextWaitTime;
    {
      std::unique_lock<std::mutex> lock(timer_mutex_);

      // 如果队列为空，等待通知
      if (timer_queue_.empty()) {
        timer_cv_.wait(lock, [this] { return !running_ || !timer_queue_.empty(); });
        if (!running_) {
          break;
        }
        if (timer_queue_.empty()) {
          continue;
        }
      }

      auto now = std::chrono::steady_clock::now();

      // 检查队首定时器是否到期
      if (now >= timer_queue_.top().expiryTime) {
        // 复制一份数据，然后立即释放锁
        expiredCondition = timer_queue_.top();
        timer_queue_.pop();
        hasExpiredCondition = true;
      } else {
        // 如果没有到期，设置等待时间
        nextWaitTime = timer_queue_.top().expiryTime;
        // 释放锁并等待到下一个到期时间
        timer_cv_.wait_until(lock, nextWaitTime);
        continue;
      }
    }
    bool expired = false;
    // 处理过期的条件
    if (hasExpiredCondition) {
      auto now = std::chrono::steady_clock::now();
      SMF_LOGD("Duration condition expired: " + expiredCondition.name + " with value " +
               std::to_string(expiredCondition.value));

      // 检查条件是否仍然满足
      std::lock_guard<std::mutex> condLock(condition_values_mutex_);
      auto it = condition_values_.find(expiredCondition.name);
      if (it != condition_values_.end()) {
        auto& condValue = it->second;
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - condValue.lastChangedTime)
                .count();
        if (condValue.value == expiredCondition.value && elapsed >= expiredCondition.duration) {
          expired = true;
          SMF_LOGI("Duration condition triggered: " + expiredCondition.name + " with value " +
                   std::to_string(expiredCondition.value));
        }
      }
    }

    // 如果条件满足，触发事件检查
    if (expired) {
      NotifyConditionChange(expiredCondition.name, expiredCondition.value,
                            expiredCondition.duration, true);
    }
  }
}

void ConditionManager::ProcessConditionUpdates() {
  std::queue<ConditionUpdateEvent> updates;
  {
    std::lock_guard<std::mutex> lock(condition_update_mutex_);
    updates.swap(condition_update_queue_);
  }

  while (!updates.empty()) {
    const auto& update = updates.front();
    bool hasDurationCondition = false;
    bool valueInRange = false;
    {
      std::lock_guard<std::mutex> lock(condition_values_mutex_);
      if (condition_values_.find(update.name) == condition_values_.end()) {
        condition_values_[update.name] = {update.name, update.value, update.updateTime,
                                          update.updateTime};
      } else {
        auto oldValue = condition_values_[update.name].value;
        condition_values_[update.name].value = update.value;
        condition_values_[update.name].lastUpdateTime = update.updateTime;
        if (oldValue != update.value) {
          condition_values_[update.name].lastChangedTime = update.updateTime;
          // 检查是否满足任何条件的范围要求
          for (const auto& cond : all_conditions_) {
            if (cond->name == update.name) {
              for (const auto& range : cond->range_values) {
                if (update.value >= range.first && update.value <= range.second) {
                  valueInRange = true;
                  break;
                }
              }
              if (cond->duration > 0 && valueInRange) {
                hasDurationCondition = true;
                std::lock_guard<std::mutex> timerLock(timer_mutex_);
                auto expiryTime = update.updateTime + std::chrono::milliseconds(cond->duration);
                timer_queue_.push({update.name, update.value, cond->duration, expiryTime});
                timer_cv_.notify_one();
                break;
              }
            }
          }
        }
      }
    }
    if (!hasDurationCondition) {
      NotifyConditionChange(update.name, update.value, 0, valueInRange);
    }

    updates.pop();
  }
}

void ConditionManager::NotifyConditionChange(const std::string& name, int value, int duration,
                                             bool meetsCondition) {
  std::lock_guard<std::mutex> lock(callback_mutex_);
  if (condition_change_callback_) {
    condition_change_callback_(name, value, duration, meetsCondition);
  }
}

}  // namespace smf