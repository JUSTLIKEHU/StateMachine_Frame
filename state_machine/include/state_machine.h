/**
 * @file state_machine.h
 * @brief State machine class definition
 * @author xiaokui.hu
 * @date 2025-01-26
 * @details This file contains the definition of the FiniteStateMachine class, which implements a
 *          finite state machine. The state machine supports state transitions based on events and
 *conditions, and allows for nested states. The state machine can be configured using a JSON file,
 *and supports callbacks for state transitions, event handling, and condition updates. The state
 *machine is thread-safe and can be used in a multi-threaded environment.
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
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common_define.h"
#include "components/condition_manager.h"
#include "components/config_loader.h"
#include "components/event_handler.h"
#include "components/state_manager.h"
#include "components/transition_manager.h"
#include "event.h"
#include "logger.h"                // 添加日志头文件
#include "nlohmann-json/json.hpp"  // 引入 nlohmann/json 库
#include "state_event_handler.h"   // 引入状态事件处理器头文件

namespace smf {

using json = nlohmann::json;

// 有限状态机类
class FiniteStateMachine final : public std::enable_shared_from_this<FiniteStateMachine> {
  friend class StateMachineFactory;

 public:
  FiniteStateMachine(const FiniteStateMachine&) = delete;
  FiniteStateMachine& operator=(const FiniteStateMachine&) = delete;
  FiniteStateMachine(FiniteStateMachine&&) = delete;
  FiniteStateMachine& operator=(FiniteStateMachine&&) = delete;
  ~FiniteStateMachine() {
    // 合并了停止定时器和状态机的逻辑
    Stop();
  }

  bool Init(const std::string& configDir);

  bool Init(const std::string& stateConfigFile, const std::string& eventGenerateConfigDir,
            const std::string& transConfigDir);

  // 启动状态机
  bool Start();

  // 停止状态机
  void Stop();

  // 处理事件（线程安全）
  void HandleEvent(const EventPtr& event);

  // 设置状态转移回调 - 函数对象版本
  void SetTransitionCallback(StateEventHandler::TransitionCallback callback);

  // 设置状态转移回调 - 类成员函数版本
  template <typename T>
  void SetTransitionCallback(T* instance,
                             void (T::*method)(const std::vector<State>&, const EventPtr&,
                                               const std::vector<State>&)) {
    if (running_) {
      SMF_LOGE("Cannot set transition callback while running.");
      return;
    }
    if (!state_event_handler_) {
      state_event_handler_ = std::make_unique<StateEventHandler>();
    }
    state_event_handler_->SetTransitionCallback(instance, method);
  }

  // 设置事件预处理回调 - 函数对象版本
  void SetPreEventCallback(StateEventHandler::PreEventCallback callback);

  // 设置事件预处理回调 - 类成员函数版本
  template <typename T>
  void SetPreEventCallback(T* instance, bool (T::*method)(const State&, const EventPtr&)) {
    if (running_) {
      SMF_LOGE("Cannot set pre event callback while running.");
      return;
    }
    if (!state_event_handler_) {
      state_event_handler_ = std::make_unique<StateEventHandler>();
    }
    state_event_handler_->SetPreEventCallback(instance, method);
  }

  // 设置状态进入回调 - 函数对象版本
  void SetEnterStateCallback(StateEventHandler::EnterStateCallback callback);

  // 设置状态进入回调 - 类成员函数版本
  template <typename T>
  void SetEnterStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    if (running_) {
      SMF_LOGE("Cannot set enter state callback while running.");
      return;
    }
    if (!state_event_handler_) {
      state_event_handler_ = std::make_unique<StateEventHandler>();
    }
    state_event_handler_->SetEnterStateCallback(instance, method);
  }

  // 设置状态退出回调 - 函数对象版本
  void SetExitStateCallback(StateEventHandler::ExitStateCallback callback);

  // 设置状态退出回调 - 类成员函数版本
  template <typename T>
  void SetExitStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    if (running_) {
      SMF_LOGE("Cannot set exit state callback while running.");
      return;
    }
    if (!state_event_handler_) {
      state_event_handler_ = std::make_unique<StateEventHandler>();
    }
    state_event_handler_->SetExitStateCallback(instance, method);
  }

  // 设置事件回收回调 - 函数对象版本
  void SetPostEventCallback(StateEventHandler::PostEventCallback callback);

  // 设置事件回收回调 - 类成员函数版本
  template <typename T>
  void SetPostEventCallback(T* instance, void (T::*method)(const EventPtr&, bool)) {
    if (running_) {
      SMF_LOGE("Cannot set post event callback while running.");
      return;
    }
    if (!state_event_handler_) {
      state_event_handler_ = std::make_unique<StateEventHandler>();
    }
    state_event_handler_->SetPostEventCallback(instance, method);
  }

  // 继续支持原有的接口，但现在作为兼容层
  void SetStateEventHandler(std::shared_ptr<StateEventHandler> handler);

  // 获取当前状态
  State GetCurrentState() const;

  // 设置条件值
  void SetConditionValue(const std::string& name, int value);

  // 获取条件值
  void GetConditionValue(const std::string& name, int& value) const;

 private:
  FiniteStateMachine(const std::string& name)
      : name_(name),
        running_(false),
        initialized_(false),
        state_event_handler_(std::make_shared<StateEventHandler>()),
        transition_manager_(std::make_unique<TransitionManager>()),
        state_manager_(std::make_unique<StateManager>()),
        condition_manager_(std::make_unique<ConditionManager>()),
        event_handler_(
            std::make_unique<EventHandler>(state_manager_.get(), condition_manager_.get(),
                                           transition_manager_.get(), state_event_handler_.get())),
        config_loader_(
            std::make_unique<ConfigLoader>(state_manager_.get(), condition_manager_.get(),
                                           transition_manager_.get(), event_handler_.get())) {
  }

 private:
  std::string name_;
  std::atomic_bool running_{false};
  std::atomic_bool initialized_{false};
  std::shared_ptr<StateEventHandler> state_event_handler_;
  // 组件
  std::unique_ptr<ITransitionManager> transition_manager_;
  std::unique_ptr<IStateManager> state_manager_;
  std::unique_ptr<IConditionManager> condition_manager_;
  std::unique_ptr<IEventHandler> event_handler_;
  std::unique_ptr<IConfigLoader> config_loader_;
};

using FiniteStateMachinePtr = std::shared_ptr<FiniteStateMachine>;

}  // namespace smf
