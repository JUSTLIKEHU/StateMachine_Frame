/**
 * @file state_manager.cpp
 * @brief Implementation of state manager component
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file contains the implementation of the StateManager class, which
 *          is responsible for tracking the current state of the state machine,
 *          managing state hierarchy, and handling state transitions.
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

#include "components/state_manager.h"

#include <algorithm>
#include <stdexcept>

#include "logger.h"

namespace smf {

StateManager::StateManager() = default;

StateManager::~StateManager() { Stop(); }

void StateManager::Start() {
  if (running_) {
    return;
  }
  running_ = true;
  timeout_thread_ = std::thread(&StateManager::StateTimeoutLoop, this);
}

void StateManager::Stop() {
  if (!running_) {
    return;
  }
  running_ = false;
  timeout_cv_.notify_all();
  if (timeout_thread_.joinable()) {
    timeout_thread_.join();
  }
}

bool StateManager::IsRunning() const { return running_; }

bool StateManager::AddStateInfo(const StateInfo& state_info) {
  if (running_) {
    SMF_LOGE("Cannot add state info while running");
    return false;
  }
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (states_.find(state_info.name) != states_.end()) {
    SMF_LOGE("State already exists: " + state_info.name);
    return false;
  }

  states_[state_info.name] = state_info;
  if (!state_info.parent.empty()) {
    auto it = states_.find(state_info.parent);
    if (it == states_.end()) {
      SMF_LOGE("Parent state does not exist: " + state_info.parent);
      return false;
    }
    it->second.children.push_back(state_info.name);
  }
  return true;
}

bool StateManager::SetState(const State& state) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (states_.find(state) == states_.end()) {
    SMF_LOGE("State does not exist: " + state);
    return false;
  }
  current_state_ = state;

  // 更新状态超时信息
  std::lock_guard<std::mutex> timeout_lock(timeout_mutex_);
  auto it = states_.find(state);
  if (it != states_.end() && it->second.timeout > 0) {
    auto now = std::chrono::steady_clock::now();
    current_state_timeout_.state = state;
    current_state_timeout_.timeout = it->second.timeout;
    current_state_timeout_.enterTime = now;
    current_state_timeout_.expiryTime = now + std::chrono::milliseconds(it->second.timeout);
    timeout_cv_.notify_one();
    SMF_LOGD("Set state timeout for state " + state + " with timeout " +
             std::to_string(it->second.timeout) + " ms");
  } else {
    current_state_timeout_.state.clear();
    current_state_timeout_.timeout = 0;
  }
  return true;
}

State StateManager::GetCurrentState() const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return current_state_;
}

std::vector<State> StateManager::GetStateHierarchy(const State& state) const {
  std::vector<State> hierarchy;
  State current = state;

  while (!current.empty()) {
    hierarchy.push_back(current);
    auto it = states_.find(current);
    if (it == states_.end()) {
      break;
    }
    current = it->second.parent;
  }

  return hierarchy;
}

void StateManager::GetStateHierarchy(const State& from, const State& to,
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
  exit_states.clear();
  for (; itFrom != fromStates.rend(); ++itFrom) {
    exit_states.push_back(*itFrom);
  }
  std::reverse(exit_states.begin(), exit_states.end());

  // 添加需要进入的状态
  enter_states.clear();
  for (; itTo != toStates.rend(); ++itTo) {
    enter_states.push_back(*itTo);
  }
}

void StateManager::RegisterStateTimeoutCallback(StateTimeoutCallback callback) {
  if (running_) {
    SMF_LOGE("StateManager is running, cannot register state timeout callback");
    return;
  }
  if (!callback) {
    SMF_LOGE("State timeout callback is nullptr");
    return;
  }
  state_timeout_callback_ = callback;
}

void StateManager::StateTimeoutLoop() {
  while (running_) {
    State timeoutState;
    bool shouldTriggerTimeout = false;

    {
      std::unique_lock<std::mutex> lock(timeout_mutex_);

      if (current_state_timeout_.state.empty() || current_state_timeout_.timeout <= 0) {
        timeout_cv_.wait(lock, [this] {
          return !running_ ||
                 (!current_state_timeout_.state.empty() && current_state_timeout_.timeout > 0);
        });

        if (!running_) {
          break;
        }
      }

      auto now = std::chrono::steady_clock::now();
      if (now >= current_state_timeout_.expiryTime) {
        timeoutState = current_state_timeout_.state;
        shouldTriggerTimeout = true;

        // 更新下一次超时时间
        current_state_timeout_.expiryTime =
            now + std::chrono::milliseconds(current_state_timeout_.timeout);
      } else {
        auto waitTime = current_state_timeout_.expiryTime;
        lock.unlock();

        std::unique_lock<std::mutex> waitLock(timeout_mutex_);
        timeout_cv_.wait_until(waitLock, waitTime, [this, waitTime] {
          return !running_ || current_state_timeout_.state.empty() ||
                 current_state_timeout_.expiryTime != waitTime;
        });

        continue;
      }
    }

    if (shouldTriggerTimeout) {
      HandleStateTimeout();
    }
  }
}

void StateManager::HandleStateTimeout() {
  SMF_LOGI("State timeout triggered for state: " + current_state_);
  if (state_timeout_callback_) {
    state_timeout_callback_(current_state_, current_state_timeout_.timeout);
  }
}

}  // namespace smf