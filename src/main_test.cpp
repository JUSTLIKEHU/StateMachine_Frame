#include <iostream>
#include "state_machine.h"


int main() {
    // 创建有限状态机
    FiniteStateMachine fsm;

    // 创建用户自定义的状态转移处理器
    auto handler = std::make_shared<LightTransitionHandler>();
    fsm.setTransitionHandler(handler);

    try {
        // 从 JSON 文件加载配置
        fsm.loadFromJSON("../../config/fsm_config.json");

        // 设置条件值
        fsm.setConditionValue("is_powered", 50);
        fsm.setConditionValue("is_connected", 75);

        // 处理事件
        fsm.handleEvent("TURN_ON");  // 从 OFF -> ON

        // 更新条件值并检查条件触发规则
        fsm.setConditionValue("is_powered", 150);
        fsm.setConditionValue("is_connected", 150);

        // 获取当前状态
        std::cout << "Current state: " << fsm.getCurrentState() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}