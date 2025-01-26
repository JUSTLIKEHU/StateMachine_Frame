
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
#include "nlohmann-json/json.hpp" // 引入 nlohmann/json 库

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
    Event event;                    // 事件
    State to;                       // 目标状态
    std::vector<Condition> conditions; // 条件列表
    std::string conditionsOperator; // 条件运算符 ("AND" 或 "OR")
};

// 状态转移处理器抽象类
class TransitionHandler {
public:
    virtual ~TransitionHandler() = default;

    // 状态转移时的回调函数
    virtual void onTransition(const State& from, const Event& event, const State& to) = 0;
};

// 有限状态机类
class FiniteStateMachine {
public:
    // 设置状态转移处理器
    void setTransitionHandler(std::shared_ptr<TransitionHandler> handler) {
        transitionHandler = handler;
    }

    // 添加状态
    void addState(const State& state) {
        states[state] = {};
    }

    // 添加状态转移规则
    void addTransition(const TransitionRule& rule) {
        if (states.find(rule.from) == states.end() || states.find(rule.to) == states.end()) {
            throw std::invalid_argument("State does not exist");
        }
        transitions[{rule.from, rule.event}] = rule;
    }

    // 设置初始状态
    void setInitialState(const State& state) {
        if (states.find(state) == states.end()) {
            throw std::invalid_argument("Initial state does not exist");
        }
        currentState = state;
    }

    // 处理事件
    void handleEvent(const Event& event) {
        auto key = std::make_pair(currentState, event);
        if (transitions.find(key) != transitions.end()) {
            const auto& rule = transitions[key];

            // 检查条件是否满足
            if (checkConditions(rule.conditions, rule.conditionsOperator)) {
                // 调用状态转移处理器的回调函数
                if (transitionHandler) {
                    transitionHandler->onTransition(currentState, event, rule.to);
                }

                currentState = rule.to; // 更新状态
                std::cout << "Transition: " << key.first << " -> " << currentState << " on event " << event << std::endl;
            } else {
                std::cout << "Conditions not met for transition from " << key.first << " on event " << event << std::endl;
            }
        } else {
            std::cout << "Invalid transition: No transition from " << currentState << " on event " << event << std::endl;
        }
    }

    // 获取当前状态
    State getCurrentState() const {
        return currentState;
    }

    // 设置条件值
    void setConditionValue(const std::string& name, int value) {
        conditionValues[name] = value;
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
            addState(state);
        }

        // 加载初始状态
        setInitialState(config["initial_state"]);

        // 加载状态转移规则
        for (const auto& transition : config["transitions"]) {
            TransitionRule rule;
            rule.from = transition["from"];
            rule.event = transition["event"];
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

    // 存储所有状态
    std::unordered_map<State, bool> states;

    // 存储状态转移规则
    std::map<std::pair<State, Event>, TransitionRule> transitions;

    // 当前状态
    State currentState;

    // 条件值
    std::unordered_map<std::string, int> conditionValues;

    // 状态转移处理器
    std::shared_ptr<TransitionHandler> transitionHandler;
};

// 用户自定义的状态转移处理器
class LightTransitionHandler : public TransitionHandler {
public:
    void onTransition(const State& from, const Event& event, const State& to) override {
        if (from == "OFF" && event == "TURN_ON" && to == "ON") {
            std::cout << "Light turned ON!" << std::endl;
        } else if (from == "ON" && event == "TURN_OFF" && to == "OFF") {
            std::cout << "Light turned OFF!" << std::endl;
        }
    }
};
