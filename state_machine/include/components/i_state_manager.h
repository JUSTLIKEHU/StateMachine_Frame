/**
 * @file i_state_manager.h
 * @brief Interface for state management
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file defines the interface for the state manager component.
 *          The state manager is responsible for tracking the current state of the
 *          state machine, managing state hierarchy, and handling state transitions.
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

#include <functional>
#include <string>
#include <vector>

#include "common_define.h"
#include "i_component.h"
#include "i_event_handler.h"

namespace smf {

class IStateManager : public IComponent {
 public:
  virtual ~IStateManager() = default;
  virtual bool AddStateInfo(const StateInfo& state_info) = 0;
  virtual bool SetState(const State& state) = 0;
  virtual State GetCurrentState() const = 0;
  virtual std::vector<State> GetStateHierarchy(const State& state) const = 0;
  virtual void GetStateHierarchy(const State& from, const State& to,
                                 std::vector<State>& exit_states,
                                 std::vector<State>& enter_states) const = 0;
  using StateTimeoutCallback = std::function<void(const State& state, int timeout)>;
  virtual void RegisterStateTimeoutCallback(StateTimeoutCallback callback) = 0;
};

}  // namespace smf