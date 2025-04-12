/**
 * @file state_machine.h
 * @brief State machine class definition
 * @date 2025-01-26
 **/

#pragma once

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

#include "nlohmann-json/json.hpp"  // 引入 nlohmann/json 库
#include "logger.h"  // 添加日志头文件

using json = nlohmann::json;

// 定义状态和事件的类型
using State = std::string;
using Event = std::string;

// 条件类型
struct Condition {
  std::string name;           // 条件名称
  std::pair<int, int> range;  // 条件范围 [min, max]
  int duration{0};            // 条件持续时间(毫秒),默认0表示立即生效
  std::chrono::steady_clock::time_point lastUpdateTime;  // 最后一次更新时间
};

// 状态转移规则
struct TransitionRule {
  State from;                         // 起始状态
  Event event;                        // 事件（可为空）
  State to;                           // 目标状态
  std::vector<Condition> conditions;  // 条件列表
  std::string conditionsOperator;     // 条件运算符 ("AND" 或 "OR")
};

// 状态信息
struct StateInfo {
  State name;                   // 状态名称
  State parent;                 // 父状态名称（可为空）
  std::vector<State> children;  // 子状态列表
};

// 在类定义之前添加条件更新事件结构体
struct ConditionUpdateEvent {
  std::string name;
  int value;
  std::chrono::steady_clock::time_point updateTime;
};

// 在ConditionUpdateEvent结构体后添加定时条件结构体
struct DurationCondition {
  std::string name;
  int value;  // 添加值字段，用于跟踪触发条件时的值
  std::chrono::steady_clock::time_point expiryTime;
};

// 重新设计状态事件处理器为回调函数集合
class StateEventHandler {
 public:
  // 各种回调函数类型定义
  using TransitionCallback = std::function<void(const std::vector<State>&, const Event&, const std::vector<State>&)>;
  using PreEventCallback = std::function<bool(const State&, const Event&)>;
  using EnterStateCallback = std::function<void(const std::vector<State>&)>;
  using ExitStateCallback = std::function<void(const std::vector<State>&)>;
  using PostEventCallback = std::function<void(const Event&, bool)>;

  // 默认构造函数
  StateEventHandler() = default;
  ~StateEventHandler() = default;

  // 设置状态转换回调
  void setTransitionCallback(TransitionCallback callback) {
    transitionCallback = std::move(callback);
  }

  // 支持类成员函数作为状态转换回调
  template<typename T>
  void setTransitionCallback(T* instance, void (T::*method)(const std::vector<State>&, const Event&, const std::vector<State>&)) {
    transitionCallback = [instance, method](const std::vector<State>& fromStates, const Event& event, const std::vector<State>& toStates) {
      (instance->*method)(fromStates, event, toStates);
    };
  }

  // 设置事件预处理回调
  void setPreEventCallback(PreEventCallback callback) {
    preEventCallback = std::move(callback);
  }

  // 支持类成员函数作为事件预处理回调
  template<typename T>
  void setPreEventCallback(T* instance, bool (T::*method)(const State&, const Event&)) {
    preEventCallback = [instance, method](const State& currentState, const Event& event) {
      return (instance->*method)(currentState, event);
    };
  }

  // 设置状态进入回调
  void setEnterStateCallback(EnterStateCallback callback) {
    enterStateCallback = std::move(callback);
  }

  // 支持类成员函数作为状态进入回调
  template<typename T>
  void setEnterStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    enterStateCallback = [instance, method](const std::vector<State>& states) {
      (instance->*method)(states);
    };
  }

  // 设置状态退出回调
  void setExitStateCallback(ExitStateCallback callback) {
    exitStateCallback = std::move(callback);
  }

  // 支持类成员函数作为状态退出回调
  template<typename T>
  void setExitStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    exitStateCallback = [instance, method](const std::vector<State>& states) {
      (instance->*method)(states);
    };
  }

  // 设置事件回收回调
  void setPostEventCallback(PostEventCallback callback) {
    postEventCallback = std::move(callback);
  }

  // 支持类成员函数作为事件回收回调
  template<typename T>
  void setPostEventCallback(T* instance, void (T::*method)(const Event&, bool)) {
    postEventCallback = [instance, method](const Event& event, bool handled) {
      (instance->*method)(event, handled);
    };
  }

  // 实际处理状态转换
  void onTransition(const std::vector<State>& fromStates, const Event& event,
                    const std::vector<State>& toStates) {
    if (transitionCallback) {
      transitionCallback(fromStates, event, toStates);
    }
  }

  // 实际处理事件预处理
  bool onPreEvent(const State& currentState, const Event& event) {
    if (preEventCallback) {
      return preEventCallback(currentState, event);
    }
    return true; // 默认允许所有事件
  }

  // 实际处理状态进入
  void onEnterState(const std::vector<State>& states) {
    if (enterStateCallback) {
      enterStateCallback(states);
    }
  }

  // 实际处理状态退出
  void onExitState(const std::vector<State>& states) {
    if (exitStateCallback) {
      exitStateCallback(states);
    }
  }

  // 实际处理事件回收
  void onPostEvent(const Event& event, bool handled) {
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

// 有限状态机类
class FiniteStateMachine {
 public:
  // 定义内部事件常量
  static constexpr const char* INTERNAL_EVENT = "INTERNAL_EVENT";

  FiniteStateMachine() : running(false), initialized(false) {}
  ~FiniteStateMachine() {
    // 合并了停止定时器和状态机的逻辑
    stop();
  }

  // 初始化接口
  bool Init(const std::string& configFile) {
    try {
      loadFromJSON(configFile);
      initialized = true;
      return true;
    } catch (const std::exception& e) {
      SMF_LOGE("Initialization failed: " + std::string(e.what()));
      return false;
    }
  }

  // 启动状态机
  bool start() {
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
    conditionThread = std::thread(&FiniteStateMachine::conditionLoop, this);
    timerThread = std::thread(&FiniteStateMachine::timerLoop, this);
    return true;
  }

  // 停止状态机
  void stop() {
    running = false;
    // 通知所有等待中的线程
    eventCV.notify_one();
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
  }

  // 处理事件（线程安全）
  void handleEvent(const Event& event) {
    std::lock_guard<std::mutex> lock(eventMutex);
    eventQueue.push(event);
    eventCV.notify_one();
  }

  // 设置状态转移回调 - 函数对象版本
  void setTransitionCallback(StateEventHandler::TransitionCallback callback) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setTransitionCallback(std::move(callback));
  }

  // 设置状态转移回调 - 类成员函数版本
  template<typename T>
  void setTransitionCallback(T* instance, void (T::*method)(const std::vector<State>&, const Event&, const std::vector<State>&)) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setTransitionCallback(instance, method);
  }

  // 设置事件预处理回调 - 函数对象版本
  void setPreEventCallback(StateEventHandler::PreEventCallback callback) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setPreEventCallback(std::move(callback));
  }

  // 设置事件预处理回调 - 类成员函数版本
  template<typename T>
  void setPreEventCallback(T* instance, bool (T::*method)(const State&, const Event&)) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setPreEventCallback(instance, method);
  }

  // 设置状态进入回调 - 函数对象版本
  void setEnterStateCallback(StateEventHandler::EnterStateCallback callback) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setEnterStateCallback(std::move(callback));
  }

  // 设置状态进入回调 - 类成员函数版本
  template<typename T>
  void setEnterStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setEnterStateCallback(instance, method);
  }

  // 设置状态退出回调 - 函数对象版本
  void setExitStateCallback(StateEventHandler::ExitStateCallback callback) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setExitStateCallback(std::move(callback));
  }

  // 设置状态退出回调 - 类成员函数版本
  template<typename T>
  void setExitStateCallback(T* instance, void (T::*method)(const std::vector<State>&)) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setExitStateCallback(instance, method);
  }

  // 设置事件回收回调 - 函数对象版本
  void setPostEventCallback(StateEventHandler::PostEventCallback callback) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setPostEventCallback(std::move(callback));
  }

  // 设置事件回收回调 - 类成员函数版本
  template<typename T>
  void setPostEventCallback(T* instance, void (T::*method)(const Event&, bool)) {
    if (!stateEventHandler) {
      stateEventHandler = std::make_shared<StateEventHandler>();
    }
    stateEventHandler->setPostEventCallback(instance, method);
  }

  // 继续支持原有的接口，但现在作为兼容层
  void setStateEventHandler(std::shared_ptr<StateEventHandler> handler) {
    stateEventHandler = handler;
  }

  // 添加状态
  void addState(const State& name, const State& parent = "") {
    if (states.find(name) != states.end()) {
      throw std::invalid_argument("State already exists: " + name);
    }

    states[name] = {name, parent, {}};
    if (!parent.empty()) {
      states[parent].children.push_back(name);
    }
  }

  // 添加状态转移规则
  void addTransition(const TransitionRule& rule) {
    if (states.find(rule.from) == states.end() || states.find(rule.to) == states.end()) {
      throw std::invalid_argument("State does not exist");
    }

    // 如果事件为空，则使用内部事件
    if (rule.event.empty()) {
      TransitionRule ruleWithInternalEvent = rule;
      ruleWithInternalEvent.event = INTERNAL_EVENT;
      eventTransitions.insert({{rule.from, INTERNAL_EVENT}, ruleWithInternalEvent});
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

  // 设置初始状态
  void setInitialState(const State& state) {
    if (states.find(state) == states.end()) {
      throw std::invalid_argument("Initial state does not exist");
    }
    currentState = state;
  }

  // 获取当前状态
  State getCurrentState() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentState;
  }

  // 修改设置条件值接口为异步处理
  void setConditionValue(const std::string& name, int value) {
    std::lock_guard<std::mutex> lock(conditionMutex);
    ConditionUpdateEvent update{name, value, std::chrono::steady_clock::now()};
    conditionQueue.push(update);
    conditionCV.notify_one(); // 通知条件处理线程
  }

  // 从 JSON 文件加载状态机配置
  void loadFromJSON(const std::string& filepath) {
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

 private:
  // 事件处理循环 - 专注于处理事件队列
  void eventLoop() {
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

  // 条件处理循环 - 专注于处理条件更新
  void conditionLoop() {
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

  // 获取状态及其所有父状态（从子到父的顺序）
  std::vector<State> getStateHierarchy(const State& state) const {
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

  // 处理单个事件
  void processEvent(const Event& event) {
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

            SMF_LOGI("Transition: " + state + " -> " + currentState + " on event " + event);
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

  // 检查条件触发规则 - 这个方法不再使用
  // 保留代码但标记为废弃，供参考
  void checkConditionTransitions() {
    // 此方法不再使用，所有状态转换都通过事件处理
  }

  // 新增：打印满足的条件
  void printSatisfiedConditions(const std::vector<Condition>& conditions) {
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

  // 检查条件是否满足
  bool checkConditions(const std::vector<Condition>& conditions, const std::string& op) {
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

  // 处理条件更新队列
  void processConditionUpdates() {
    bool immediateConditionChanged = false;
    
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
        
        // 如果没有持续时间条件，标记为需要立即检查
        if (!hasDurationCondition) {
          immediateConditionChanged = true;
        }
      }
      conditionQueue.pop();
    }
    
    // 只有即时条件发生变化时，才触发内部事件进行条件检查
    if (immediateConditionChanged) {
      handleEvent(INTERNAL_EVENT);
    }
  }

  // 新增定时器循环处理
  void timerLoop() {
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
            // 触发内部事件进行条件检查
            handleEvent(INTERNAL_EVENT);
          }
        }
      } else {
        timerCV.wait_until(lock, timerQueue.top().expiryTime);
      }
    }
  }

  // 存储所有状态
  std::unordered_map<State, StateInfo> states;

  // 存储事件触发的状态转移规则 - 修改为multimap允许同一状态下存在多个同名事件的转移规则
  std::multimap<std::pair<State, Event>, TransitionRule> eventTransitions;

  // 当前状态
  State currentState;

  // 条件值
  std::unordered_map<std::string, int> conditionValues;

  // 状态转移处理器
  std::shared_ptr<StateEventHandler> stateEventHandler;

  // 新增成员变量
  bool running;
  bool initialized;
  std::thread eventThread;
  std::thread conditionThread;
  std::thread timerThread;
  std::queue<Event> eventQueue;
  std::mutex eventMutex;
  std::condition_variable eventCV;
  std::queue<ConditionUpdateEvent> conditionQueue;
  std::mutex conditionMutex;
  std::condition_variable conditionCV;
  mutable std::mutex stateMutex;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> conditionLastUpdate;
  // 添加新的成员变量来存储所有条件
  std::vector<Condition> allConditions;
  std::priority_queue<DurationCondition, std::vector<DurationCondition>,
                      std::function<bool(const DurationCondition&, const DurationCondition&)>>
      timerQueue{[](const DurationCondition& lhs, const DurationCondition& rhs) {
        return lhs.expiryTime > rhs.expiryTime;
      }};
  std::mutex timerMutex;
  std::condition_variable timerCV;
  std::unordered_map<std::string, int> triggeredDurationConditions; // 记录已触发的持续条件
};

// 创建一个演示如何使用类成员函数作为回调的示例类
class LightController {
public:
  LightController() : powerOn(false) {}

  // 转换处理
  void handleTransition(const std::vector<State>& fromStates, const Event& event, 
                      const std::vector<State>& toStates) {
    (void)event;  // 防止未使用警告
    
    // 获取当前状态（层次结构中的第一个）
    State from = fromStates.empty() ? "" : fromStates[0];
    State to = toStates.empty() ? "" : toStates[0];
    
    if (from == "OFF" && to == "ON") {
      SMF_LOGI("Controller: Light turned ON!");
      powerOn = true;
    } else if (from == "ON" && to == "OFF") {
      SMF_LOGI("Controller: Light turned OFF!");
      powerOn = false;
    }
  }

  // 事件预处理
  bool validateEvent(const State& state, const Event& event) {
    SMF_LOGD("Controller: Validating event " + event + " in state " + state);
    // 例如，仅在灯开启时允许"ADJUST_BRIGHTNESS"事件
    if (event == "ADJUST_BRIGHTNESS" && state != "ON") {
      SMF_LOGW("Controller: Cannot adjust brightness when light is off!");
      return false;
    }
    return true;
  }

  // 状态进入
  void onEnter(const std::vector<State>& states) {
    if (!states.empty()) {
      SMF_LOGD("Controller: Entered state " + states[0]);
      if (states[0] == "ON") {
        // 可以执行实际的硬件操作，如通过GPIO打开灯
        SMF_LOGI("Controller: Powering on hardware...");
      }
    }
  }

  // 状态退出
  void onExit(const std::vector<State>& states) {
    if (!states.empty()) {
      SMF_LOGD("Controller: Exited state " + states[0]);
    }
  }

  // 事件后处理
  void afterEvent(const Event& event, bool handled) {
    SMF_LOGD("Controller: Processed event " + event + 
              (handled ? " successfully" : " but it was not handled"));
  }

  bool isPowerOn() const { return powerOn; }

private:
  bool powerOn;
};

// 更新示例函数，展示如何创建使用类成员函数的处理器
inline std::shared_ptr<StateEventHandler> createMemberFunctionHandler() {
  auto controller = std::make_shared<LightController>();
  auto handler = std::make_shared<StateEventHandler>();
  
  // 绑定类成员函数作为回调
  handler->setTransitionCallback(controller.get(), &LightController::handleTransition);
  handler->setPreEventCallback(controller.get(), &LightController::validateEvent);
  handler->setEnterStateCallback(controller.get(), &LightController::onEnter);
  handler->setExitStateCallback(controller.get(), &LightController::onExit);
  handler->setPostEventCallback(controller.get(), &LightController::afterEvent);
  
  // 注意：这里我们返回了处理器，但要确保controller对象的生命周期必须超过处理器的使用时间
  // 在实际应用中，可能需要让控制器对象的所有权与处理器或状态机的生命周期绑定
  return handler;
}

// 创建一个配置灯光状态处理逻辑的示例函数
inline std::shared_ptr<StateEventHandler> createLightStateHandler() {
  auto handler = std::make_shared<StateEventHandler>();
  
  // 设置转换回调
  handler->setTransitionCallback([](const std::vector<State>& fromStates, const Event& event, 
                                    const std::vector<State>& toStates) {
    (void)event;  // 防止未使用警告
    
    // 获取当前状态（层次结构中的第一个）
    State from = fromStates.empty() ? "" : fromStates[0];
    State to = toStates.empty() ? "" : toStates[0];
    
    // 处理逻辑
    if (from == "OFF" && to == "ACTIVE") {
      SMF_LOGI("Light turned ON and is ACTIVE!");
    } else if (from == "ON" && to == "OFF") {
      SMF_LOGI("Light turned OFF!");
    }
    
    // 打印状态层次
    std::string transition = "Complete transition: ";
    for (const auto& s : fromStates) {
      transition += s + " ";
    }
    transition += "-> ";
    for (const auto& s : toStates) {
      transition += s + " ";
    }
    SMF_LOGD(transition);
  });
  
  // 设置事件预处理回调
  handler->setPreEventCallback([](const State& currentState, const Event& event) {
    SMF_LOGD("Pre-processing event: " + event + " in state: " + currentState);
    if (event == "unsupported_event") {
      SMF_LOGW("Rejecting unsupported event!");
      return false;
    }
    return true;
  });
  
  // 设置状态进入回调
  handler->setEnterStateCallback([](const std::vector<State>& states) {
    if (states.empty()) return;
    
    SMF_LOGD("Entering state: " + states[0]);
    if (states[0] == "ON") {
      SMF_LOGI("Turning ON the light!");
    } else if (states[0] == "OFF") {
      SMF_LOGI("Light is now OFF!");
    }
  });
  
  // 设置状态退出回调
  handler->setExitStateCallback([](const std::vector<State>& states) {
    if (states.empty()) return;
    
    SMF_LOGD("Exiting state: " + states[0]);
    if (states[0] == "ON") {
      SMF_LOGI("Preparing to turn OFF the light...");
    }
  });
  
  // 设置事件回收回调
  handler->setPostEventCallback([](const Event& event, bool handled) {
    SMF_LOGD("Post-processing event: " + event +
              (handled ? " (handled)" : " (not handled)"));
  });
  
  return handler;
}
