/**
 * @file transition_manager.h
 * @brief Transition manager implementation
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file contains the implementation of the transition manager component,
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

#pragma once

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "i_transition_manager.h"

namespace smf {

class TransitionManager : public ITransitionManager {
 public:
  TransitionManager();
  ~TransitionManager() override;

  // IComponent interface
  void Start() override;
  void Stop() override;
  bool IsRunning() const override;

  // ITransitionManager interface
  bool AddTransition(const TransitionRuleSharedPtr& rule) override;
  bool FindTransition(const State& current_state, const EventPtr& event,
                      std::vector<TransitionRuleSharedPtr>& out_rules) override;
  void Clear() override;

 private:
  // 使用状态ID和事件类型作为键的复合键结构
  struct TransitionKey {
    std::string state_id;
    std::string event_type;

    bool operator==(const TransitionKey& other) const {
      return state_id == other.state_id && event_type == other.event_type;
    }
  };

  // 为TransitionKey提供哈希函数
  struct TransitionKeyHash {
    std::size_t operator()(const TransitionKey& key) const {
      return std::hash<std::string>()(key.state_id) ^
             (std::hash<std::string>()(key.event_type) << 1);
    }
  };

  // 存储转换规则
  std::unordered_multimap<TransitionKey, TransitionRuleSharedPtr, TransitionKeyHash> transitions_;

  // 使用共享互斥锁实现读写锁
  mutable std::shared_mutex mutex_;
  std::atomic_bool running_{false};
};

}  // namespace smf