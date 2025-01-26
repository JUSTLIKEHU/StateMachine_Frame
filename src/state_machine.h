
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
    void setTransitionHandler(TransitionHandler* handler) {
        transitionHandler = handler;
    }

    // 添加状态
    void addState(const State& state) {
        states[state] = {};
    }

    // 添加状态转移规则
    void addTransition(const State& from, const Event& event, const State& to) {
        if (states.find(from) == states.end() || states.find(to) == states.end()) {
            throw std::invalid_argument("State does not exist");
        }
        transitions[{from, event}] = to;
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
            State nextState = transitions[key];

            // 调用状态转移处理器的回调函数
            if (transitionHandler) {
                transitionHandler->onTransition(currentState, event, nextState);
            }

            currentState = nextState; // 更新状态
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
    std::map<std::pair<State, Event>, State> transitions;

    // 当前状态
    State currentState;

    // 状态转移处理器
    TransitionHandler* transitionHandler = nullptr;
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
