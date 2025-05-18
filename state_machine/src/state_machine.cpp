/**
 *@file   state_machine.cpp
 *@brief  State machine implementation
 *@author xiaokui.hu
 *@date   2025-04-13
 *@details This file contains the implementation of the FiniteStateMachine class, which implements a
 *         finite state machine.
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

#include "state_machine.h"

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "common_define.h"
#include "event.h"
#include "logger.h"

namespace smf {

bool FiniteStateMachine::Init(const std::string& configDir) {
  if (initialized_) {
    SMF_LOGW("State machine already initialized!");
    return true;
  }

  if (!config_loader_) {
    SMF_LOGE("Config loader not initialized!");
    return false;
  }

  if (!config_loader_->LoadConfig(configDir)) {
    SMF_LOGE("Failed to load state config: " + configDir);
    return false;
  }
  initialized_ = true;
  return true;
}

bool FiniteStateMachine::Init(const std::string& stateConfigFile,
                              const std::string& eventGenerateConfigDir,
                              const std::string& transConfigDir) {
  if (initialized_) {
    SMF_LOGW("State machine already initialized!");
    return true;
  }

  if (!config_loader_) {
    SMF_LOGE("Config loader not initialized!");
    return false;
  }

  if (!config_loader_->LoadStateConfig(stateConfigFile)) {
    SMF_LOGE("Failed to load state config: " + stateConfigFile);
    return false;
  }

  if (!config_loader_->LoadEventConfig(eventGenerateConfigDir)) {
    SMF_LOGE("Failed to load event config: " + eventGenerateConfigDir);
    return false;
  }

  if (!config_loader_->LoadTransitionConfig(transConfigDir)) {
    SMF_LOGE("Failed to load transition config: " + transConfigDir);
    return false;
  }
  initialized_ = true;
  return true;
}

bool FiniteStateMachine::Start() {
  if (!initialized_) {
    SMF_LOGE("State machine not initialized!");
    return false;
  }

  if (running_) {
    SMF_LOGW("State machine already running!");
    return false;
  }
  config_loader_->Start();
  event_handler_->Start();
  condition_manager_->Start();
  state_manager_->Start();
  transition_manager_->Start();
  running_ = true;
  return true;
}

void FiniteStateMachine::Stop() {
  config_loader_->Stop();
  running_ = false;
  event_handler_->Stop();
  condition_manager_->Stop();
  state_manager_->Stop();
  transition_manager_->Stop();
}

void FiniteStateMachine::HandleEvent(const EventPtr& event) { event_handler_->HandleEvent(event); }

void FiniteStateMachine::SetTransitionCallback(StateEventHandler::TransitionCallback callback) {
  if (running_) {
    SMF_LOGE("Cannot set transition callback while running.");
    return;
  }
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetTransitionCallback(std::move(callback));
}

void FiniteStateMachine::SetPreEventCallback(StateEventHandler::PreEventCallback callback) {
  if (running_) {
    SMF_LOGE("Cannot set pre event callback while running.");
    return;
  }
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetPreEventCallback(std::move(callback));
}

void FiniteStateMachine::SetEnterStateCallback(StateEventHandler::EnterStateCallback callback) {
  if (running_) {
    SMF_LOGE("Cannot set enter state callback while running.");
    return;
  }
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetEnterStateCallback(std::move(callback));
}

void FiniteStateMachine::SetExitStateCallback(StateEventHandler::ExitStateCallback callback) {
  if (running_) {
    SMF_LOGE("Cannot set exit callback callback while running.");
    return;
  }
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetExitStateCallback(std::move(callback));
}

void FiniteStateMachine::SetPostEventCallback(StateEventHandler::PostEventCallback callback) {
  if (running_) {
    SMF_LOGE("Cannot set post event callback while running.");
    return;
  }
  if (!state_event_handler_) {
    state_event_handler_ = std::make_shared<StateEventHandler>();
  }
  state_event_handler_->SetPostEventCallback(std::move(callback));
}

void FiniteStateMachine::SetStateEventHandler(std::shared_ptr<StateEventHandler> handler) {
  if (running_) {
    SMF_LOGE("Cannot set state event handler while running.");
    return;
  }
  
  if (!handler) {
    SMF_LOGE("Cannot set null state event handler.");
    return;
  }
  
  state_event_handler_ = handler;
}

State FiniteStateMachine::GetCurrentState() const { return state_manager_->GetCurrentState(); }

void FiniteStateMachine::SetConditionValue(const std::string& name, int value) {
  condition_manager_->SetConditionValue(name, value);
}

void FiniteStateMachine::GetConditionValue(const std::string& name, int& value) const {
  condition_manager_->GetConditionValue(name, value);
}

}  // namespace smf
