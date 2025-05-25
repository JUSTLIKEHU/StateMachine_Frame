/**
 * @file condition_manager.h
 * @brief Condition manager implementation
 * @date 2025-05-17
 * @details This file contains the implementation of the condition manager component.
 *          It handles the management of conditions and their values, including
 *          condition value updates, condition checking, and condition change notifications.
 *          The condition manager is designed to be thread-safe and can be used in a multi-threaded environment.
 * @author xiaokui.hu
 * @version 1.0
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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "i_condition_manager.h"

namespace smf {

class ConditionManager : public IConditionManager {
 public:
  ConditionManager();
  ~ConditionManager();

  // IComponent interface
  void Start() override;
  void Stop() override;
  bool IsRunning() const override;

  // IConditionManager interface
  void SetConditionValue(const std::string& name, int value) override;
  bool CheckConditions(const std::vector<ConditionSharedPtr>& conditions, const std::string& op,
                       std::vector<ConditionInfo>& condition_infos) override;
  void AddCondition(const ConditionSharedPtr& condition) override;
  void GetConditionValue(const std::string& name, int& value) const override;
  void RegisterConditionChangeCallback(ConditionChangeCallback callback) override;

 private:
  void ConditionLoop();
  void TimerLoop();
  void ProcessConditionUpdates();
  void NotifyConditionChange(const std::string& name, int value, int duration, bool meetsCondition);

 private:
  std::atomic_bool running_{false};

  // 条件相关
  std::vector<ConditionSharedPtr> all_conditions_;
  std::unordered_map<std::string, ConditionValue> condition_values_;
  mutable std::mutex condition_values_mutex_;

  // 条件更新队列
  std::queue<ConditionUpdateEvent> condition_update_queue_;
  std::mutex condition_update_mutex_;
  std::condition_variable condition_update_cv_;
  std::thread condition_thread_;

  std::priority_queue<DurationCondition, std::vector<DurationCondition>,
                      std::function<bool(const DurationCondition&, const DurationCondition&)>>
      timer_queue_{[](const DurationCondition& lhs, const DurationCondition& rhs) {
        return lhs.expiryTime > rhs.expiryTime;
      }};
  std::mutex timer_mutex_;
  std::condition_variable timer_cv_;
  std::thread timer_thread_;

  // 条件变化回调
  ConditionChangeCallback condition_change_callback_;
  std::mutex callback_mutex_;
};

}  // namespace smf