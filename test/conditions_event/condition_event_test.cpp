/**
 * @file condition_event_test.cpp
 * @brief 条件事件测试程序
 * @author xiaokui.hu
 * @date 2025-04-13
 * @details 用于测试通过条件配置并触发事件的功能
 **/

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "state_machine.h"

using namespace smf;

// 状态变化回调
void OnStateChanged(const std::vector<State>& fromStates, const Event& event, 
                   const std::vector<State>& toStates) {
    State from = fromStates.empty() ? "" : fromStates[0];
    State to = toStates.empty() ? "" : toStates[0];
    
    std::cout << "[状态变化] " << from << " -> " << to;
    if (!event.empty()) {
        std::cout << " (由事件 " << event << " 触发)";
    }
    std::cout << std::endl;
}

int main() {
    // 配置状态机
    FiniteStateMachine stateMachine;
    std::string configPath = "../../test/conditions_event/config/condition_event_test.json";
    
    // 初始化状态机
    if (!stateMachine.Init(configPath)) {
        std::cerr << "初始化状态机失败！" << std::endl;
        return 1;
    }
    
    // 设置状态变化回调
    stateMachine.setTransitionCallback(OnStateChanged);
    
    // 启动状态机
    if (!stateMachine.start()) {
        std::cerr << "启动状态机失败！" << std::endl;
        return 1;
    }
    
    std::cout << "状态机已启动，初始状态: " << stateMachine.getCurrentState() << std::endl;
    
    // 测试场景1: 正常启动工作流程
    std::cout << "\n[测试场景1] 正常启动工作流程" << std::endl;
    std::cout << "设置 power=1" << std::endl;
    stateMachine.setConditionValue("power", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::cout << "设置 system_ready=1" << std::endl;
    stateMachine.setConditionValue("system_ready", 1);
    
    // 等待足够的时间让持续条件满足
    std::cout << "等待条件持续时间满足..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    
    std::cout << "当前状态: " << stateMachine.getCurrentState() << std::endl;
    
    // 测试场景2: 条件不满足导致事件消失
    std::cout << "\n[测试场景2] 条件不满足导致事件消失" << std::endl;
    std::cout << "设置 system_ready=0" << std::endl;
    stateMachine.setConditionValue("system_ready", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "当前状态: " << stateMachine.getCurrentState() << std::endl;
    
    // 测试场景3: 触发错误
    std::cout << "\n[测试场景3] 触发系统错误" << std::endl;
    std::cout << "设置 power=1, system_ready=1" << std::endl;
    stateMachine.setConditionValue("power", 1);
    stateMachine.setConditionValue("system_ready", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    
    std::cout << "当前状态: " << stateMachine.getCurrentState() << std::endl;
    
    std::cout << "设置 error_code=5" << std::endl;
    stateMachine.setConditionValue("error_code", 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "当前状态: " << stateMachine.getCurrentState() << std::endl;
    
    // 测试场景4: 清除错误恢复初始状态
    std::cout << "\n[测试场景4] 清除错误恢复初始状态" << std::endl;
    std::cout << "设置 error_code=0" << std::endl;
    stateMachine.setConditionValue("error_code", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "当前状态: " << stateMachine.getCurrentState() << std::endl;
    
    // 停止状态机
    stateMachine.stop();
    std::cout << "测试完成，状态机已停止" << std::endl;
    
    return 0;
}