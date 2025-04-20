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

#include "logger.h"
#include "state_machine.h"

using namespace smf;

// 状态变化回调
void OnStateChanged(const std::vector<State>& fromStates, const Event& event,
                    const std::vector<State>& toStates) {
  State from = fromStates.empty() ? "" : fromStates[0];
  State to = toStates.empty() ? "" : toStates[0];

  std::string logMsg = "[状态变化]: " + from + " -> " + to;
  if (!event.empty()) {
    logMsg += " (由事件 " + event.GetName() + " 触发)";
  }
  SMF_LOGI(logMsg);
}

int main() {
  // 初始化日志系统
  SMF_LOGGER_INIT(smf::LogLevel::DEBUG);
  // 配置状态机
  FiniteStateMachine stateMachine;
  std::string configPath = "../../test/conditions_event/config/condition_event_test.json";

  // 初始化状态机
  if (!stateMachine.Init(configPath)) {
    SMF_LOGE("初始化状态机失败！");
    return 1;
  }

  // 设置状态变化回调
  stateMachine.SetTransitionCallback(OnStateChanged);

  // 启动状态机
  if (!stateMachine.Start()) {
    SMF_LOGE("启动状态机失败！");
    return 1;
  }

  SMF_LOGI("状态机已启动，初始状态: " + stateMachine.GetCurrentState());

  // 测试场景1: 正常启动工作流程
  SMF_LOGI("\n[测试场景1] 正常启动工作流程");
  SMF_LOGI("设置 power=1");
  stateMachine.SetConditionValue("power", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  SMF_LOGI("设置 power=0");
  stateMachine.SetConditionValue("power", 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(700));

  SMF_LOGI("设置 power=1");
  stateMachine.SetConditionValue("power", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  SMF_LOGI("设置 system_ready=1");
  stateMachine.SetConditionValue("system_ready", 1);

  // 等待足够的时间让持续条件满足
  SMF_LOGI("等待条件持续时间满足...");
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  SMF_LOGI("当前状态: " + stateMachine.GetCurrentState());

  // 测试场景2: 条件不满足导致事件消失
  SMF_LOGI("\n[测试场景2] 条件不满足导致事件消失");
  SMF_LOGI("设置 system_ready=0");
  stateMachine.SetConditionValue("system_ready", 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  SMF_LOGI("当前状态: " + stateMachine.GetCurrentState());

  // 测试场景3: 触发错误
  SMF_LOGI("\n[测试场景3] 触发系统错误");
  SMF_LOGI("设置 power=1, system_ready=1");
  stateMachine.SetConditionValue("power", 1);
  stateMachine.SetConditionValue("system_ready", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));

  SMF_LOGI("当前状态: " + stateMachine.GetCurrentState());

  SMF_LOGI("设置 error_code=5");
  stateMachine.SetConditionValue("error_code", 5);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  SMF_LOGI("当前状态: " + stateMachine.GetCurrentState());

  // 测试场景4: 清除错误恢复初始状态
  SMF_LOGI("\n[测试场景4] 清除错误恢复初始状态");
  SMF_LOGI("设置 error_code=0");
  stateMachine.SetConditionValue("error_code", 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  SMF_LOGI("当前状态: " + stateMachine.GetCurrentState());

  // 停止状态机
  stateMachine.Stop();
  SMF_LOGI("测试完成，状态机已停止");

  return 0;
}