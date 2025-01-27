#include <iostream>
#include "state_machine.h"
#include <thread>
#include <chrono>

int main() {
    FiniteStateMachine fsm;
    auto handler = std::make_shared<LightTransitionHandler>();
    fsm.setTransitionHandler(handler);

    try {
        // 初始化状态机
        if (!fsm.Init("../../config/fsm_config.json")) {
            std::cerr << "Failed to initialize state machine" << std::endl;
            return 1;
        }

        // 启动状态机
        if (!fsm.start()) {
            std::cerr << "Failed to start state machine" << std::endl;
            return 1;
        }

        // 异步设置条件和发送事件
        fsm.setConditionValue("is_powered", 50);
        fsm.setConditionValue("is_connected", 75);
        fsm.handleEvent("TURN_ON");

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        fsm.setConditionValue("is_powered", 150);
        fsm.setConditionValue("is_connected", 150);
        fsm.handleEvent("TURN_OFF");

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::cout << "Current state: " << fsm.getCurrentState() << std::endl;

        // 停止状态机
        fsm.stop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}