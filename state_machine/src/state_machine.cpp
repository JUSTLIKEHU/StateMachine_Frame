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
#include <stdexcept>
#include <vector>

#include "common_define.h"
#include "components/condition_manager.h"
#include "components/config_loader.h"
#include "components/event_handler.h"
#include "components/state_manager.h"
#include "components/transition_manager.h"
#include "event.h"
#include "logger.h"

namespace smf {

FiniteStateMachine::FiniteStateMachine(const std::string& name)
    : name_(name),
      running_(false),
      initialized_(false),
      state_event_handler_(std::make_shared<StateEventHandler>()),
      transition_manager_(std::make_unique<TransitionManager>()),
      state_manager_(std::make_unique<StateManager>()),
      condition_manager_(std::make_unique<ConditionManager>()),
      event_handler_(std::make_unique<EventHandler>(state_manager_.get(), condition_manager_.get(),
                                                    transition_manager_.get(),
                                                    state_event_handler_)),
      config_loader_(std::make_unique<ConfigLoader>(state_manager_.get(), condition_manager_.get(),
                                                    transition_manager_.get(),
                                                    event_handler_.get())) {}

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
  
  // 同步更新 EventHandler 中的处理器
  auto* eventHandler = dynamic_cast<EventHandler*>(event_handler_.get());
  if (eventHandler) {
    eventHandler->SetStateEventHandler(handler);
  }
}

State FiniteStateMachine::GetCurrentState() const { return state_manager_->GetCurrentState(); }

void FiniteStateMachine::SetConditionValue(const std::string& name, int value) {
  condition_manager_->SetConditionValue(name, value);
}

void FiniteStateMachine::GetConditionValue(const std::string& name, int& value) const {
  condition_manager_->GetConditionValue(name, value);
}

}  // namespace smf
