
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

// 定义状态转移时的回调函数类型
using TransitionCallback = std::function<void()>;

// 有限状态机类
class FiniteStateMachine {
public:
    // 添加状态
    void addState(const State& state) {
        states[state] = {};
    }

    // 添加状态转移规则
    void addTransition(const State& from, const Event& event, const State& to, TransitionCallback callback = nullptr) {
        if (states.find(from) == states.end() || states.find(to) == states.end()) {
            throw std::invalid_argument("State does not exist");
        }
        transitions[{from, event}] = {to, callback};
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
            auto& transition = transitions[key];
            currentState = transition.first; // 更新状态
            if (transition.second) {
                transition.second(); // 执行回调函数
            }
            std::cout << "Transition: " << key.first << " -> " << currentState << " on event " << event << std::endl;
        } else {
            std::cout << "Invalid transition: No transition from " << currentState << " on event " << event << std::endl;
        }
    }

    // 获取当前状态
    State getCurrentState() const {
        return currentState;
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
            State from = transition["from"];
            Event event = transition["event"];
            State to = transition["to"];
            addTransition(from, event, to);
        }
    }

private:
    // 存储所有状态
    std::unordered_map<State, bool> states;

    // 存储状态转移规则
    std::map<std::pair<State, Event>, std::pair<State, TransitionCallback>> transitions;

    // 当前状态
    State currentState;
};

// 示例回调函数
void onTurnOn() {
    std::cout << "Light turned ON!" << std::endl;
}

void onTurnOff() {
    std::cout << "Light turned OFF!" << std::endl;
}