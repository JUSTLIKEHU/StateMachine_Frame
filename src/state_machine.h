/**
  * @file state_machine.h
  * @brief State machine class definition
  * @date 2025-01-26
**/

#pragma once

#include <iostream>
#include <unordered_map>
#include <functional>
#include <string>
#include <map>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <memory>
#include "nlohmann-json/json.hpp" // 引入 nlohmann/json 库
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

using json = nlohmann::json;

// 定义状态和事件的类型
using State = std::string;
using Event = std::string;

// 条件类型
struct Condition {
    std::string name;       // 条件名称
    std::pair<int, int> range; // 条件范围 [min, max]
};

// 状态转移规则
struct TransitionRule {
    State from;                     // 起始状态
    Event event;                    // 事件（可为空）
    State to;                       // 目标状态
    std::vector<Condition> conditions; // 条件列表
    std::string conditionsOperator; // 条件运算符 ("AND" 或 "OR")
};

// 状态信息
struct StateInfo {
    State name;                     // 状态名称
    State parent;                   // 父状态名称（可为空）
    std::vector<State> children;    // 子状态列表
};

// 状态转移处理器抽象类
class TransitionHandler {
public:
    virtual ~TransitionHandler() = default;

    // 状态转移时的回调函数
    virtual void onTransition(const State& from, const Event& event, const State& to) = 0;
};

// 在类定义之前添加条件更新事件结构体
struct ConditionUpdateEvent {
    std::string name;
    int value;
};

// 有限状态机类
class FiniteStateMachine {
public:
    FiniteStateMachine() : running(false), initialized(false) {}
    ~FiniteStateMachine() {
        stop();
    }

    // 初始化接口
    bool Init(const std::string& configFile) {
        try {
            loadFromJSON(configFile);
            initialized = true;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Initialization failed: " << e.what() << std::endl;
            return false;
        }
    }

    // 启动状态机
    bool start() {
        if (!initialized) {
            std::cerr << "State machine not initialized!" << std::endl;
            return false;
        }
        
        if (running) {
            std::cerr << "State machine already running!" << std::endl;
            return false;
        }

        running = true;
        workerThread = std::thread(&FiniteStateMachine::processLoop, this);
        return true;
    }

    // 停止状态机
    void stop() {
        running = false;
        eventCV.notify_one();
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    // 处理事件（新版本 - 线程安全）
    void handleEvent(const Event& event) {
        std::lock_guard<std::mutex> lock(eventMutex);
        eventQueue.push(event);
        eventCV.notify_one();
    }

    // 设置状态转移处理器
    void setTransitionHandler(std::shared_ptr<TransitionHandler> handler) {
        transitionHandler = handler;
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

        // 如果事件为空，则添加到条件触发规则列表
        if (rule.event.empty()) {
            conditionTransitions[rule.from].push_back(rule);
        } else {
            eventTransitions[{rule.from, rule.event}] = rule;
        }

        // 初始化条件的默认值为 0
        for (const auto& cond : rule.conditions) {
            if (conditionValues.find(cond.name) == conditionValues.end()) {
                conditionValues[cond.name] = 0; // 默认值为 0
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
        return currentState;
    }

    // 修改设置条件值接口为异步处理
    void setConditionValue(const std::string& name, int value) {
        std::lock_guard<std::mutex> lock(conditionMutex);
        conditionQueue.push({name, value});
        eventCV.notify_one();
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
            rule.event = transition.value("event", ""); // 事件可为空
            rule.to = transition["to"];
            rule.conditionsOperator = transition.value("conditions_operator", "AND");

            // 加载条件
            if (transition.contains("conditions")) {
                for (const auto& condition : transition["conditions"]) {
                    Condition cond;
                    cond.name = condition["name"];
                    cond.range = {condition["range"][0], condition["range"][1]};
                    rule.conditions.push_back(cond);
                }
            }

            addTransition(rule);
        }
    }

private:
    // 修改处理循环，同时处理事件和条件更新
    void processLoop() {
        while (running) {
            // 处理条件更新和事件
            {
                std::unique_lock<std::mutex> lock(eventMutex);
                eventCV.wait(lock, [this]() {
                    return !eventQueue.empty() || !conditionQueue.empty() || !running;
                });

                if (!running) break;

                // 优先处理条件更新
                processConditionUpdates();

                // 处理事件队列
                if (!eventQueue.empty()) {
                    Event event = eventQueue.front();
                    eventQueue.pop();
                    processEvent(event);
                }
            }
            
            // 检查条件触发的转换
            checkConditionTransitions();
        }
    }

    // 处理单个事件
    void processEvent(const Event& event) {
        // 将原来 handleEvent 的核心逻辑移到这里
        State state = currentState;
        while (!state.empty()) {
            auto key = std::make_pair(state, event);
            if (eventTransitions.find(key) != eventTransitions.end()) {
                const auto& rule = eventTransitions[key];
                if (checkConditions(rule.conditions, rule.conditionsOperator)) {
                    if (transitionHandler) {
                        transitionHandler->onTransition(currentState, event, rule.to);
                    }
                    currentState = rule.to;
                    std::cout << "Transition: " << state << " -> " << currentState 
                             << " on event " << event << std::endl;
                    return;
                }
            }
            state = states[state].parent;
        }
    }

    // 检查条件触发规则
    void checkConditionTransitions() {
        // 检查当前状态及其父状态的转移规则
        State state = currentState;
        while (!state.empty()) {
            if (conditionTransitions.find(state) != conditionTransitions.end()) {
                for (const auto& rule : conditionTransitions[state]) {
                    // 检查条件是否满足
                    if (checkConditions(rule.conditions, rule.conditionsOperator)) {
                        // 调用状态转移处理器的回调函数
                        if (transitionHandler) {
                            transitionHandler->onTransition(currentState, "", rule.to);
                        }

                        currentState = rule.to; // 更新状态
                        std::cout << "Condition-based transition: " << state << " -> " << currentState << std::endl;
                        return; // 一次只触发一个转移
                    }
                }
            }

            // 检查父状态
            state = states[state].parent;
        }
    }

    // 检查条件是否满足
    bool checkConditions(const std::vector<Condition>& conditions, const std::string& op) {
        if (conditions.empty()) {
            return true; // 无条件限制
        }

        for (const auto& cond : conditions) {
            if (conditionValues.find(cond.name) == conditionValues.end()) {
                throw std::invalid_argument("Condition value not set: " + cond.name);
            }

            int value = conditionValues[cond.name];
            bool isMet = (value >= cond.range.first && value <= cond.range.second);

            if (op == "AND" && !isMet) {
                return false; // AND 条件不满足
            } else if (op == "OR" && isMet) {
                return true; // OR 条件满足
            }
        }

        return (op == "AND"); // AND 条件全部满足，或 OR 条件全部不满足
    }

    // 新增：处理条件更新队列
    void processConditionUpdates() {
        std::lock_guard<std::mutex> lock(conditionMutex);
        while (!conditionQueue.empty()) {
            const auto& update = conditionQueue.front();
            conditionValues[update.name] = update.value;
            conditionQueue.pop();
        }
    }

    // 存储所有状态
    std::unordered_map<State, StateInfo> states;

    // 存储事件触发的状态转移规则
    std::map<std::pair<State, Event>, TransitionRule> eventTransitions;

    // 存储条件触发的状态转移规则
    std::unordered_map<State, std::vector<TransitionRule>> conditionTransitions;

    // 当前状态
    State currentState;

    // 条件值
    std::unordered_map<std::string, int> conditionValues;

    // 状态转移处理器
    std::shared_ptr<TransitionHandler> transitionHandler;

    // 新增成员变量
    bool running;
    bool initialized;
    std::thread workerThread;
    std::queue<Event> eventQueue;
    std::mutex eventMutex;
    std::condition_variable eventCV;
    std::queue<ConditionUpdateEvent> conditionQueue;
    std::mutex conditionMutex;
};

// 用户自定义的状态转移处理器
class LightTransitionHandler : public TransitionHandler {
public:
    void onTransition(const State& from, const Event& event, const State& to) override {
        (void)event; // 防止未使用的警告
        if (from == "OFF" && to == "ACTIVE") {
            std::cout << "Light turned ON and is ACTIVE!" << std::endl;
        } else if (from == "ON" && to == "OFF") {
            std::cout << "Light turned OFF!" << std::endl;
        }
    }
};
