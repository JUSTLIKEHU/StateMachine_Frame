/**
 * @file state_hierarchy_test.cpp
 * @brief 测试FiniteStateMachine的GetStateHierarchy接口功能
 * @date 2025-04-20
 **/

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include "logger.h"
#include "state_machine.h"

// 打印状态层次结构
void printStateHierarchy(const std::vector<smf::State>& hierarchy, const std::string& title) {
    std::cout << title << ":" << std::endl;
    for (size_t i = 0; i < hierarchy.size(); ++i) {
        std::cout << std::setw(4) << i << ": " << hierarchy[i] << std::endl;
    }
    std::cout << std::endl;
}

// 打印状态转换路径
void printTransitionPath(const std::vector<smf::State>& exitStates, 
                         const std::vector<smf::State>& enterStates,
                         const std::string& from,
                         const std::string& to) {
    std::cout << "从状态 [" << from << "] 转换到 [" << to << "] 的路径:" << std::endl;
    
    if (!exitStates.empty()) {
        std::cout << "需要退出的状态:" << std::endl;
        for (const auto& state : exitStates) {
            std::cout << "  - " << state << std::endl;
        }
    } else {
        std::cout << "无需退出任何状态" << std::endl;
    }
    
    if (!enterStates.empty()) {
        std::cout << "需要进入的状态:" << std::endl;
        for (const auto& state : enterStates) {
            std::cout << "  - " << state << std::endl;
        }
    } else {
        std::cout << "无需进入任何状态" << std::endl;
    }
    
    std::cout << std::endl;
}

// 创建测试用的状态机配置
void setupStateMachine(smf::FiniteStateMachine& fsm) {
    // 添加状态及其层次结构
    
    // 根状态
    fsm.AddState("ROOT", "");
    
    // 第一级子状态
    fsm.AddState("A", "ROOT");
    fsm.AddState("B", "ROOT");
    
    // A的子状态
    fsm.AddState("A1", "A");
    fsm.AddState("A2", "A");
    
    // A1的子状态
    fsm.AddState("A1a", "A1");
    fsm.AddState("A1b", "A1");
    
    // A2的子状态
    fsm.AddState("A2a", "A2");
    fsm.AddState("A2b", "A2");
    
    // B的子状态
    fsm.AddState("B1", "B");
    fsm.AddState("B2", "B");
}

// 测试单个状态的层次结构
void testSingleStateHierarchy(const smf::FiniteStateMachine& fsm) {
    std::cout << "========== 测试单个状态的层次结构 ==========" << std::endl;
    
    // 测试各种不同深度的状态
    auto rootHierarchy = fsm.GetStateHierarchy("ROOT");
    printStateHierarchy(rootHierarchy, "ROOT的层次结构");
    
    auto aHierarchy = fsm.GetStateHierarchy("A");
    printStateHierarchy(aHierarchy, "A的层次结构");
    
    auto a1Hierarchy = fsm.GetStateHierarchy("A1");
    printStateHierarchy(a1Hierarchy, "A1的层次结构");
    
    auto a1aHierarchy = fsm.GetStateHierarchy("A1a");
    printStateHierarchy(a1aHierarchy, "A1a的层次结构");
    
    auto b2Hierarchy = fsm.GetStateHierarchy("B2");
    printStateHierarchy(b2Hierarchy, "B2的层次结构");
}

// 测试两个状态之间的层次结构差异
void testStateTransitionHierarchy(const smf::FiniteStateMachine& fsm) {
    std::cout << "========== 测试状态转换的层次结构 ==========" << std::endl;
    
    std::vector<std::pair<std::string, std::string>> transitions = {
        {"A1a", "A1b"},    // 同一父状态下的兄弟状态
        {"A1a", "A2b"},    // 不同父状态，但共享祖父状态
        {"A1a", "B1"},     // 完全不同的分支
        {"A", "A1a"},      // 父状态到子状态
        {"A1a", "A"},      // 子状态到父状态
        {"ROOT", "B2"},    // 根状态到深层子状态
        {"A1a", "A1a"}     // 相同状态
    };
    
    for (const auto& transition : transitions) {
        std::vector<smf::State> exitStates;
        std::vector<smf::State> enterStates;
        
        fsm.GetStateHierarchy(transition.first, transition.second, exitStates, enterStates);
        printTransitionPath(exitStates, enterStates, transition.first, transition.second);
    }
}

// 验证退出和进入状态的顺序
void testStateTransitionOrder(const smf::FiniteStateMachine& fsm) {
    std::cout << "========== 验证状态转换顺序 ==========" << std::endl;
    
    // 从深层状态A1a到深层状态B2的转换
    std::vector<smf::State> exitStates;
    std::vector<smf::State> enterStates;
    
    fsm.GetStateHierarchy("A1a", "B2", exitStates, enterStates);
    
    std::cout << "从A1a到B2的转换路径:" << std::endl;
    
    std::cout << "退出顺序 (应该是从具体到一般):" << std::endl;
    for (size_t i = 0; i < exitStates.size(); ++i) {
        std::cout << i+1 << ". " << exitStates[i] << std::endl;
    }
    
    std::cout << "进入顺序 (应该是从一般到具体):" << std::endl;
    for (size_t i = 0; i < enterStates.size(); ++i) {
        std::cout << i+1 << ". " << enterStates[i] << std::endl;
    }
}

// 主函数
int main() {
    // 初始化日志系统
    SMF_LOGGER_INIT(smf::LogLevel::INFO);
    
    try {
        // 创建状态机
        smf::FiniteStateMachine fsm;
        
        // 设置状态机
        setupStateMachine(fsm);
        
        // 测试单个状态的层次结构
        testSingleStateHierarchy(fsm);
        
        // 测试状态转换的层次结构
        testStateTransitionHierarchy(fsm);
        
        // 测试状态转换顺序
        testStateTransitionOrder(fsm);
        
        std::cout << "GetStateHierarchy接口测试完成!" << std::endl;
        
    } catch (const std::exception& e) {
        SMF_LOGE("错误: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}