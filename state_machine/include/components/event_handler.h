/**
 * @file event_handler.h
 * @brief Event handler implementation
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file contains the implementation of the event handler component,
 *          which processes events in the state machine and determines if they
 *          trigger state transitions based on the current state and conditions.
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

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "i_condition_manager.h"
#include "i_event_handler.h"
#include "i_state_manager.h"
#include "i_transition_manager.h"
#include "state_event_handler.h"

namespace smf {

class EventHandler : public IEventHandler {
 public:
  EventHandler(IStateManager* state_manager, IConditionManager* condition_manager,
               ITransitionManager* transition_manager, StateEventHandler* state_event_handler);
  ~EventHandler();

  // IComponent interface
  void Start() override;
  void Stop() override;
  bool IsRunning() const override;

  // IEventHandler interface
  void HandleEvent(const EventPtr& event) override;
  bool AddEventDefinition(const EventDefinition& event_definition) override;

 private:
  void EventLoop();
  void ProcessEvent(const EventPtr& event);
  void TriggerEvent(const std::string& condition_name, int value, int duration,
                    bool value_in_range);
  void TriggerStateTimeoutEvent(const State& state, int timeout);
  void PrintSatisfiedConditions(const std::vector<ConditionInfo>& condition_infos) const;

 private:
  std::atomic_bool running_{false};
  std::queue<EventPtr> event_queue_;
  std::mutex event_mutex_;
  std::condition_variable event_cv_;
  std::thread event_thread_;

  std::vector<EventDefinition> event_definitions_;

  // 依赖的其他组件
  IStateManager* state_manager_;
  IConditionManager* condition_manager_;
  ITransitionManager* transition_manager_;
  StateEventHandler* state_event_handler_;
};

}  // namespace smf