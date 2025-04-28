/**
 * @file state_event_handler.h
 * @brief State event handler for the state machine.
 * @details This file contains the definition of the StateEventHandler class, which is responsible
 * for handling events in the state machine.
 * @author xiaokui.hu
 * @note This class is designed to be used with the FiniteStateMachine class.
 *       It provides methods for handling state transitions, entering and exiting states, and
 * processing events.
 * @date 2025-04-13
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

#include <functional>

#include "common_define.h"
#include "event.h"

namespace smf {
class StateEventHandler final {
 public:
  // 各种回调函数类型定义
  using TransitionCallback =
      std::function<void(const std::vector<State>&, const EventPtr&, const std::vector<State>&)>;
  using PreEventCallback = std::function<bool(const State&, const EventPtr&)>;
  using EnterStateCallback = std::function<void(const std::vector<State>&)>;
  using ExitStateCallback = std::function<void(const std::vector<State>&)>;
  using PostEventCallback = std::function<void(const EventPtr&, bool)>;

  // 默认构造函数
  StateEventHandler() = default;
  ~StateEventHandler() = default;
  // 禁止拷贝构造和赋值
  StateEventHandler(const StateEventHandler&) = delete;
  StateEventHandler& operator=(const StateEventHandler&) = delete;
  // 禁止移动构造和赋值
  StateEventHandler(StateEventHandler&&) = delete;
  StateEventHandler& operator=(StateEventHandler&&) = delete;

  // 设置状态转换回调
  void SetTransitionCallback(TransitionCallback callback) {
    transitionCallback = std::move(callback);
  }

  // 支持类成员函数作为状态转换回调
  template <typename T>
  void SetTransitionCallback(T* instance,
                             void (T::*method)(const std::vector<State>&, const EventPtr&,
                                               const std::vector<State>&)) {
    transitionCallback = [instance, method](const std::vector<State>& fromStates,
                                            const EventPtr& event,
                                            const std::vector<State>& toStates) {
      (instance->*method)(fromStates, event, toStates);
    };
  }

  // 设置事件预处理回调
  void SetPreEventCallback(PreEventCallback callback) { preEventCallback = std::move(callback); }

  // 支持类成员函数作为事件预处理回调
  template <typename T>
  void SetPreEventCallback(T* instance, bool (T::*method)(const State&, const EventPtr&)) {
    preEventCallback = [instance, method](const State& currentState, const EventPtr& event) {
      return (instance->*method)(currentState, event);
    };
  }

  // 设置状态进入回调
  void SetEnterStateCallback(EnterStateCallback callback) {
    enterStateCallback = std::move(callback);
  }

  // 支持类成员函数作为状态进入回调
  template <typename T>
  void SetEnterStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    enterStateCallback = [instance, method](const std::vector<State>& states) {
      (instance->*method)(states);
    };
  }

  // 设置状态退出回调
  void SetExitStateCallback(ExitStateCallback callback) { exitStateCallback = std::move(callback); }

  // 支持类成员函数作为状态退出回调
  template <typename T>
  void SetExitStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    exitStateCallback = [instance, method](const std::vector<State>& states) {
      (instance->*method)(states);
    };
  }

  // 设置事件回收回调
  void SetPostEventCallback(PostEventCallback callback) { postEventCallback = std::move(callback); }

  // 支持类成员函数作为事件回收回调
  template <typename T>
  void SetPostEventCallback(T* instance, void (T::*method)(const EventPtr&, bool)) {
    postEventCallback = [instance, method](const EventPtr& event, bool handled) {
      (instance->*method)(event, handled);
    };
  }

  // 实际处理状态转换
  void OnTransition(const std::vector<State>& fromStates, const EventPtr& event,
                    const std::vector<State>& toStates) {
    if (transitionCallback) {
      transitionCallback(fromStates, event, toStates);
    }
  }

  // 实际处理事件预处理
  bool OnPreEvent(const State& currentState, const EventPtr& event) {
    if (preEventCallback) {
      return preEventCallback(currentState, event);
    }
    return true;  // 默认允许所有事件
  }

  // 实际处理状态进入
  void OnEnterState(const std::vector<State>& states) {
    if (enterStateCallback) {
      enterStateCallback(states);
    }
  }

  // 实际处理状态退出
  void OnExitState(const std::vector<State>& states) {
    if (exitStateCallback) {
      exitStateCallback(states);
    }
  }

  // 实际处理事件回收
  void OnPostEvent(const EventPtr& event, bool handled) {
    if (postEventCallback) {
      postEventCallback(event, handled);
    }
  }

 private:
  TransitionCallback transitionCallback;
  PreEventCallback preEventCallback;
  EnterStateCallback enterStateCallback;
  ExitStateCallback exitStateCallback;
  PostEventCallback postEventCallback;
};
}  // namespace smf
