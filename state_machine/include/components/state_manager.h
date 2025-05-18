/**
 * @file state_manager.h
 * @brief State manager implementation
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file contains the implementation of the state manager component,
 *          which is responsible for tracking the current state of the state machine,
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

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "common_define.h"
#include "i_event_handler.h"
#include "i_state_manager.h"

namespace smf {

class StateManager : public IStateManager {
 public:
  StateManager();
  ~StateManager();

  // IComponent interface
  void Start() override;
  void Stop() override;
  bool IsRunning() const override;

  // IStateManager interface
  bool AddStateInfo(const StateInfo& state_info) override;
  bool SetState(const State& state) override;
  State GetCurrentState() const override;
  std::vector<State> GetStateHierarchy(const State& state) const override;
  void GetStateHierarchy(const State& from, const State& to, std::vector<State>& exit_states,
                         std::vector<State>& enter_states) const override;
  void RegisterStateTimeoutCallback(StateTimeoutCallback callback) override;
 private:
  void StateTimeoutLoop();
  void HandleStateTimeout();

 private:
  std::atomic_bool running_{false};

  // 状态相关
  std::unordered_map<State, StateInfo> states_;
  State current_state_;
  mutable std::mutex state_mutex_;

  // 状态超时相关
  StateTimeoutInfo current_state_timeout_;
  std::mutex timeout_mutex_;
  std::condition_variable timeout_cv_;
  std::thread timeout_thread_;

  // 状态超时回调
  StateTimeoutCallback state_timeout_callback_;
};

}  // namespace smf