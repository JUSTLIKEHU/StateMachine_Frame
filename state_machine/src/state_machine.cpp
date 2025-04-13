/**
 *@file   state_machine.cpp
 *@brief  State machine implementation
 *@author xiaokui.hu
 *@date   2025-04-13
 *@details This file contains the implementation of the FiniteStateMachine class, which implements a finite state machine.
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
#include <mutex>

namespace smf {

bool FiniteStateMachine::Init(const std::string& configFile) {
  try {
    loadFromJSON(configFile);
    initialized = true;
    return true;
  } catch (const std::exception& e) {
    SMF_LOGE("Initialization failed: " + std::string(e.what()));
    return false;
  }
}

bool FiniteStateMachine::start() {
  if (!initialized) {
    SMF_LOGE("State machine not initialized!");
    return false;
  }

  if (running) {
    SMF_LOGW("State machine already running!");
    return false;
  }

  running = true;
  eventThread = std::thread(&FiniteStateMachine::eventLoop, this);
  eventTriggerThread = std::thread(&FiniteStateMachine::eventTriggerLoop, this);
  conditionThread = std::thread(&FiniteStateMachine::conditionLoop, this);
  timerThread = std::thread(&FiniteStateMachine::timerLoop, this);
  return true;
}

void FiniteStateMachine::stop() {
  running = false;
  // 通知所有等待中的线程
  eventCV.notify_one();
  eventTriggerCV.notify_one();
  conditionCV.notify_one();
  timerCV.notify_one();

  // 等待线程结束
  if (timerThread.joinable()) {
    timerThread.join();
  }
  if (conditionThread.joinable()) {
    conditionThread.join();
  }
  if (eventThread.joinable()) {
    eventThread.join();
  }
  if (eventTriggerThread.joinable()) {
    eventTriggerThread.join();
  }
}

void FiniteStateMachine::handleEvent(const Event& event) {
  std::lock_guard<std::mutex> lock(eventMutex);
  eventQueue.push(event);
  eventCV.notify_one();
}

void FiniteStateMachine::setTransitionCallback(StateEventHandler::TransitionCallback callback) {
  if (!stateEventHandler) {
    stateEventHandler = std::make_shared<StateEventHandler>();
  }
  stateEventHandler->setTransitionCallback(std::move(callback));
}

void FiniteStateMachine::setPreEventCallback(StateEventHandler::PreEventCallback callback) {
  if (!stateEventHandler) {
    stateEventHandler = std::make_shared<StateEventHandler>();
  }
  stateEventHandler->setPreEventCallback(std::move(callback));
}

void FiniteStateMachine::setEnterStateCallback(StateEventHandler::EnterStateCallback callback) {
  if (!stateEventHandler) {
    stateEventHandler = std::make_shared<StateEventHandler>();
  }
  stateEventHandler->setEnterStateCallback(std::move(callback));
}

void FiniteStateMachine::setExitStateCallback(StateEventHandler::ExitStateCallback callback) {
  if (!stateEventHandler) {
    stateEventHandler = std::make_shared<StateEventHandler>();
  }
  stateEventHandler->setExitStateCallback(std::move(callback));
}

void FiniteStateMachine::setPostEventCallback(StateEventHandler::PostEventCallback callback) {
  if (!stateEventHandler) {
    stateEventHandler = std::make_shared<StateEventHandler>();
  }
  stateEventHandler->setPostEventCallback(std::move(callback));
}

void FiniteStateMachine::setStateEventHandler(std::shared_ptr<StateEventHandler> handler) {
  stateEventHandler = handler;
}

void FiniteStateMachine::addState(const State& name, const State& parent) {
  if (states.find(name) != states.end()) {
    throw std::invalid_argument("State already exists: " + name);
  }

  states[name] = {name, parent, {}};
  if (!parent.empty()) {
    states[parent].children.push_back(name);
  }
}

void FiniteStateMachine::addTransition(const TransitionRule& rule) {
  if (states.find(rule.from) == states.end() || states.find(rule.to) == states.end()) {
    throw std::invalid_argument("State does not exist");
  }

  // 如果事件为空，则使用内部事件
  if (rule.event.empty()) {
    TransitionRule ruleWithInternalEvent = rule;
    ruleWithInternalEvent.event = Event(INTERNAL_EVENT);
    eventTransitions.insert({{rule.from, ruleWithInternalEvent.event}, ruleWithInternalEvent});
  } else {
    eventTransitions.insert({{rule.from, rule.event}, rule});
  }

  // 初始化条件的默认值为 0
  for (const auto& cond : rule.conditions) {
    if (conditionValues.find(cond.name) == conditionValues.end()) {
      conditionValues[cond.name] = 0;  // 默认值为 0
    }
  }
}

void FiniteStateMachine::setInitialState(const State& state) {
  if (states.find(state) == states.end()) {
    throw std::invalid_argument("Initial state does not exist");
  }
  currentState = state;
}

State FiniteStateMachine::getCurrentState() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return currentState;
}

void FiniteStateMachine::setConditionValue(const std::string& name, int value) {
  std::lock_guard<std::mutex> lock(conditionMutex);
  ConditionUpdateEvent update{name, value, std::chrono::steady_clock::now()};
  conditionQueue.push(update);
  conditionCV.notify_one(); // 通知条件处理线程
}

void FiniteStateMachine::loadFromJSON(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open JSON file");
  }

  json config;
  file >> config;

  // 加载状态
  for (const auto& state : config["states"]) {
    State name = state["name"];
    State parent = state.value("parent", "");
    addState(name, parent);
  }

  // 加载初始状态
  setInitialState(config["initial_state"]);

  // 加载事件定义（新增）
  if (config.contains("events")) {
    for (const auto& eventJson : config["events"]) {
      EventDefinition eventDef;
      eventDef.name = eventJson["name"];
      eventDef.trigger_mode = eventJson.value("trigger_mode", "edge"); // 默认为边缘触发
      eventDef.conditionsOperator = eventJson.value("conditions_operator", "AND"); // 默认为AND条件
      
      // 加载条件
      if (eventJson.contains("conditions")) {
        for (const auto& condition : eventJson["conditions"]) {
          Condition cond;
          cond.name = condition["name"];
          cond.range = {condition["range"][0], condition["range"][1]};
          cond.duration = condition.value("duration", 0);  // 默认为0
          eventDef.conditions.push_back(cond);
          allConditions.push_back(cond);
        }
      }
      
      // 为每个事件创建同名条件，初始值为0
      conditionValues[eventDef.name] = 0;
      
      // 添加到事件定义列表
      eventDefinitions.push_back(eventDef);
    }
  }

  // 加载状态转移规则
  for (const auto& transition : config["transitions"]) {
    TransitionRule rule;
    rule.from = transition["from"];
    rule.event = transition.value("event", "");  // 事件可为空
    rule.to = transition["to"];
    rule.conditionsOperator = transition.value("conditions_operator", "AND");

    // 加载条件
    if (transition.contains("conditions")) {
      for (const auto& condition : transition["conditions"]) {
        Condition cond;
        cond.name = condition["name"];
        cond.range = {condition["range"][0], condition["range"][1]};
        cond.duration = condition.value("duration", 0);  // 默认为0
        rule.conditions.push_back(cond);
        allConditions.push_back(cond);
      }
    }

    addTransition(rule);
  }
}

void FiniteStateMachine::eventLoop() {
  while (running) {
    Event event;
    {
      std::unique_lock<std::mutex> lock(eventMutex);
      eventCV.wait(lock, [this] { return !running || !eventQueue.empty(); });
      
      if (!running) break;
      
      if (!eventQueue.empty()) {
        event = eventQueue.front();
        eventQueue.pop();
      } else {
        continue;
      }
    }
    
    // 处理事件
    processEvent(event);
  }
}

void FiniteStateMachine::conditionLoop() {
  while (running) {
    {
      std::unique_lock<std::mutex> lock(conditionMutex);
      conditionCV.wait(lock, [this] { return !running || !conditionQueue.empty(); });
      
      if (!running) break;
      
      if (!conditionQueue.empty()) {
        processConditionUpdates();
      }
    }
  }
}

std::vector<State> FiniteStateMachine::getStateHierarchy(const State& state) const {
  std::vector<State> hierarchy;
  State current = state;

  while (!current.empty()) {
    hierarchy.push_back(current);
    auto it = states.find(current);
    if (it == states.end()) break;
    current = it->second.parent;
  }

  return hierarchy;
}

void FiniteStateMachine::processEvent(const Event& event) {
  bool eventHandled = false;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    State state = currentState;

    // 事件预处理
    if (stateEventHandler && !stateEventHandler->onPreEvent(currentState, event)) {
      // 事件被拒绝处理
      if (stateEventHandler) {
        stateEventHandler->onPostEvent(event, false);
      }
      return;
    }

    while (!state.empty()) {
      auto key = std::make_pair(state, event);
      
      // 查找当前状态和事件的所有转移规则
      auto range = eventTransitions.equal_range(key);
      for (auto it = range.first; it != range.second; ++it) {
        const auto& rule = it->second;
        if (checkConditions(rule.conditions, rule.conditionsOperator)) {
          // 获取起始状态和目标状态的完整层次结构
          std::vector<State> fromStates = getStateHierarchy(currentState);
          std::vector<State> toStates = getStateHierarchy(rule.to);

          // 调用状态退出处理
          if (stateEventHandler) {
            stateEventHandler->onExitState(fromStates);
          }

          // 执行状态转换
          if (stateEventHandler) {
            stateEventHandler->onTransition(fromStates, event, toStates);
          }

          State prevState = currentState;
          currentState = rule.to;

          // 调用状态进入处理
          if (stateEventHandler) {
            stateEventHandler->onEnterState(toStates);
          }

          SMF_LOGI("Transition: " + state + " -> " + currentState + " on event " + event.getName());
          printSatisfiedConditions(rule.conditions);

          eventHandled = true;
          break;
        }
      }
      if (eventHandled) {
        break;  // 找到匹配的转移规则，退出循环
      }
      state = states[state].parent;
    }
  }
  // 事件回收处理
  if (stateEventHandler) {
    stateEventHandler->onPostEvent(event, eventHandled);
  }
}

void FiniteStateMachine::checkConditionTransitions() {
  // 此方法不再使用，所有状态转换都通过事件处理
}

void FiniteStateMachine::printSatisfiedConditions(const std::vector<Condition>& conditions) {
  std::lock_guard<std::mutex> lock(conditionMutex);
  if (!conditions.empty()) {
    SMF_LOGD("Satisfied conditions:");
    for (const auto& cond : conditions) {
      int value = conditionValues[cond.name];
      if (value >= cond.range.first && value <= cond.range.second) {
        SMF_LOGD("  - " + cond.name + " = " + std::to_string(value) + " (range: [" + 
                std::to_string(cond.range.first) + ", " + std::to_string(cond.range.second) + 
                "],duration: " + std::to_string(cond.duration) + ")");
      }
    }
  }
}

bool FiniteStateMachine::checkConditions(const std::vector<Condition>& conditions, const std::string& op) {
  std::lock_guard<std::mutex> lock(conditionMutex);
  if (conditions.empty()) {
    return true;  // 无条件限制
  }

  auto now = std::chrono::steady_clock::now();

  for (const auto& cond : conditions) {
    if (conditionValues.find(cond.name) == conditionValues.end()) {
      throw std::invalid_argument("Condition value not set: " + cond.name);
    }

    int value = conditionValues[cond.name];
    bool valueInRange = (value >= cond.range.first && value <= cond.range.second);

    // 检查持续时间
    if (cond.duration > 0 && valueInRange) {
      // 如果条件配置了持续时间，检查当前时间是否超过持续时间
      // 检查是否在已触发的持续条件中
      auto it = triggeredDurationConditions.find(cond.name);
      if (it != triggeredDurationConditions.end() && it->second == value) {
        // 此持续条件已经满足并触发过，无需再次等待
        if (op == "OR") {
          return true;  // OR 条件满足
        }
        continue;  // 继续检查下一个条件
      }

      // 否则需要检查计时器
      if (conditionLastUpdate.find(cond.name) != conditionLastUpdate.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - conditionLastUpdate[cond.name])
                           .count();
        bool durationMet = (elapsed >= cond.duration);

        // 只有时间条件满足时才算此条件满足
        valueInRange = durationMet;

        // 如果时间条件满足，记录此条件已触发
        if (durationMet) {
          triggeredDurationConditions[cond.name] = value;
        }
      } else {
        // 如果没有更新记录，设置为不满足
        valueInRange = false;
      }
    }

    if (op == "AND" && !valueInRange) {
      return false;  // AND 条件不满足
    } else if (op == "OR" && valueInRange) {
      return true;  // OR 条件满足
    }
  }

  return (op == "AND");  // AND 条件全部满足，或 OR 条件全部不满足
}

void FiniteStateMachine::processConditionUpdates() {
  
  while (!conditionQueue.empty()) {
    const auto& update = conditionQueue.front();
    int oldValue = conditionValues[update.name];
    conditionValues[update.name] = update.value;
    
    // 只有当值改变时才进行后续处理
    if (oldValue != update.value) {
      conditionLastUpdate[update.name] = update.updateTime;
      // 如果之前有触发记录且值改变，清除触发记录
      if (triggeredDurationConditions.find(update.name) != triggeredDurationConditions.end()) {
        triggeredDurationConditions.erase(update.name);
      }
      
      bool hasDurationCondition = false;
      
      // 检查此条件值是否配置了持续时间
      for (const auto& cond : allConditions) {
        if (cond.name == update.name) {
          if (cond.duration > 0 && 
              (update.value >= cond.range.first && update.value <= cond.range.second)) {
            // 如果条件配置了持续时间且当前值在范围内，添加到定时器
            std::lock_guard<std::mutex> timerLock(timerMutex);
            auto expiryTime = update.updateTime + std::chrono::milliseconds(cond.duration);
            timerQueue.push({update.name, update.value, expiryTime});
            timerCV.notify_one();
            hasDurationCondition = true;
            break; // 一个条件名只添加一个定时器
          }
        }
      }
      
      // 如果没有持续时间条件，需要触发事件生成检查
      if (!hasDurationCondition) {
        eventTriggerCV.notify_one();
        SMF_LOGD("Condition updated: " + update.name + " to " + std::to_string(update.value));
      }
    }
    conditionQueue.pop();
  }
}

void FiniteStateMachine::triggerEvent() {
  // 检查所有事件定义
  for (const auto& eventDef : eventDefinitions) {
    bool conditionsMet = checkConditions(eventDef.conditions, eventDef.conditionsOperator);
    {
      std::lock_guard<std::mutex> lock(conditionMutex);
      int currentEventConditionValue = conditionValues[eventDef.name];
      // 检查事件条件是否满足
      if (conditionsMet) {
        // 如果条件满足，且对应事件条件当前值为0（边缘触发）
        if (currentEventConditionValue == 0) {
          // 更新事件同名条件值为1
          conditionValues[eventDef.name] = 1;
          // 记录更新时间
          conditionLastUpdate[eventDef.name] = std::chrono::steady_clock::now();
          
          SMF_LOGI("Event condition met: " + eventDef.name + ", triggered event and set condition to 1");
          
          // 触发事件
          if (eventDef.trigger_mode == "edge") {
            // 创建事件并包含所有相关条件值
            Event newEvent(eventDef.name);
            for (const auto& cond : eventDef.conditions) {
              // 使用统一的setCondition接口，同时处理条件值和持续时间
              newEvent.setCondition(cond.name, conditionValues[cond.name], cond.duration > 0 ? cond.duration : 0);
            }
            handleEvent(newEvent);
          }
        }
      } else {
        // 条件不满足，且对应事件条件当前值为1（表示之前条件满足过）
        if (currentEventConditionValue == 1) {
          // 将事件同名条件值重置为0
          conditionValues[eventDef.name] = 0;
          // 记录更新时间
          conditionLastUpdate[eventDef.name] = std::chrono::steady_clock::now();
          
          SMF_LOGI("Event condition no longer met: " + eventDef.name + ", reset condition to 0");
          
          // 如果是边缘触发模式，在条件消失时也触发一次事件
          if (eventDef.trigger_mode == "edge") {
            // 这里可以选择触发特定的事件，例如 eventDef.name + "_RESET"
            // 或者直接触发内部事件进行状态检查
            handleEvent(Event(INTERNAL_EVENT));
          }
        }
      }
    }
  }
  // 所有条件更新都支持触发内部事件
  handleEvent(Event(INTERNAL_EVENT));
}

void FiniteStateMachine::timerLoop() {
  while (running) {
    std::unique_lock<std::mutex> lock(timerMutex);
    if (timerQueue.empty()) {
      timerCV.wait(lock, [this] { return !running || !timerQueue.empty(); });
      continue;
    }
    auto now = std::chrono::steady_clock::now();
    if (now >= timerQueue.top().expiryTime) {
      auto expiredCondition = timerQueue.top();
      timerQueue.pop();
      SMF_LOGD("Duration condition expired: " + expiredCondition.name + 
              " with value " + std::to_string(expiredCondition.value));
      // 检查当前条件值是否仍然匹配触发时的值
      {
        std::lock_guard<std::mutex> condLock(conditionMutex);
        if (conditionValues.find(expiredCondition.name) != conditionValues.end() &&
            conditionValues[expiredCondition.name] == expiredCondition.value) {
          // 记录此条件已触发
          triggeredDurationConditions[expiredCondition.name] = expiredCondition.value;
          SMF_LOGI("Duration condition triggered: " + expiredCondition.name + 
                  " with value " + std::to_string(expiredCondition.value));
          // 触发事件检查
          std::lock_guard<std::mutex> eventLock(eventTriggerMutex);
          eventTriggerCV.notify_one();
        }
      }
    } else {
      timerCV.wait_until(lock, timerQueue.top().expiryTime);
    }
  }
}

void FiniteStateMachine::eventTriggerLoop() {
  while (running) {
    std::unique_lock<std::mutex> lock(eventTriggerMutex);
    eventTriggerCV.wait(lock);
    if (!running) break;
    // 触发事件
    triggerEvent();
  }
}

} // namespace smf