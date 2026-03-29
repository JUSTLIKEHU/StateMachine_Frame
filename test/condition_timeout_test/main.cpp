#include <chrono>
#include <iostream>
#include <thread>

#include "logger.h"
#include "state_machine.h"
#include "state_machine_factory.h"

using namespace smf;

class TimeoutTestHandler {
 public:
  void OnTransition(const std::vector<State>& exitStates, const EventPtr& event,
                    const std::vector<State>& enterStates) {
    std::string exitStatesStr;
    for (const auto& state : exitStates) {
      exitStatesStr += state + " ";
    }
    std::string enterStatesStr;
    for (const auto& state : enterStates) {
      enterStatesStr += state + " ";
    }
    SMF_LOGI("状态转移: " + exitStatesStr + "-> " + enterStatesStr + "事件: " + event->GetName());
  }

  void OnEnterState(const std::vector<State>& states) {
    std::string statesStr;
    for (const auto& state : states) {
      statesStr += state + " ";
    }
    SMF_LOGI("进入状态: " + statesStr);
  }

  void OnExitState(const std::vector<State>& states) {
    std::string statesStr;
    for (const auto& state : states) {
      statesStr += state + " ";
    }
    SMF_LOGI("退出状态: " + statesStr);
  }

  bool OnPreEvent(const State& currentState, const EventPtr& event) {
    SMF_LOGI("预处理事件: " + event->GetName() + " 在状态: " + currentState);
    return true;
  }

  void OnPostEvent(const EventPtr& event, bool handled) {
    SMF_LOGI("事件处理完成: " + event->GetName() + " 处理结果: " + (handled ? "成功" : "失败"));
  }
};

int main() {
  SMF_LOGI("=== 状态机Timeout功能测试 ===");

  // 创建状态机
  auto stateMachine = StateMachineFactory::CreateStateMachine("TimeoutTest");
  if (!stateMachine) {
    SMF_LOGE("创建状态机失败");
    return -1;
  }

  // 设置回调处理器
  TimeoutTestHandler handler;
  stateMachine->SetTransitionCallback(&handler, &TimeoutTestHandler::OnTransition);
  stateMachine->SetEnterStateCallback(&handler, &TimeoutTestHandler::OnEnterState);
  stateMachine->SetExitStateCallback(&handler, &TimeoutTestHandler::OnExitState);
  stateMachine->SetPreEventCallback(&handler, &TimeoutTestHandler::OnPreEvent);
  stateMachine->SetPostEventCallback(&handler, &TimeoutTestHandler::OnPostEvent);

  // 初始化状态机配置
  if (!stateMachine->Init("../../test/condition_timeout_test/config")) {
    SMF_LOGE("初始化状态机失败");
    return -1;
  }

  // 启动状态机
  if (!stateMachine->Start()) {
    SMF_LOGE("启动状态机失败");
    return -1;
  }

  SMF_LOGI("当前状态: " + stateMachine->GetCurrentState());

  // 测试场景1: 触发事件但条件不满足，配置了timeout
  SMF_LOGI("=== 测试场景1: 触发事件但条件不满足，配置了timeout ===");

  // 设置条件值，使其不满足转移条件
  stateMachine->SetConditionValue("temperature", 10);  // 假设需要温度>20才能转移

  // 触发事件
  auto event1 = std::make_shared<Event>("start_heating");
  stateMachine->HandleEvent(event1);

  SMF_LOGI("等待1秒...");
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // 测试场景2: 在timeout期间条件满足
  SMF_LOGI("=== 测试场景2: 在timeout期间条件满足 ===");

  // 更新条件值，使其满足转移条件
  stateMachine->SetConditionValue("temperature", 25);

  SMF_LOGI("等待1秒观察状态转移...");
  std::this_thread::sleep_for(std::chrono::seconds(1));

  SMF_LOGI("当前状态: " + stateMachine->GetCurrentState());

  // 测试场景3: timeout超时
  SMF_LOGI("=== 测试场景3: timeout超时测试 ===");

  // 设置条件值，使其不满足转移条件
  stateMachine->SetConditionValue("pressure", 5);  // 假设需要压力>10才能转移

  // 触发事件
  auto event2 = std::make_shared<Event>("increase_pressure");
  stateMachine->HandleEvent(event2);

  std::thread([&]() {
    SMF_LOGI("等待2.5秒...");
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    SMF_LOGI("设置温度为100...");
    stateMachine->SetConditionValue("temperature", 100);
    // stateMachine->HandleEvent(event1);
  }).detach();

  SMF_LOGI("等待3秒观察timeout...");
  std::this_thread::sleep_for(std::chrono::seconds(3));
  SMF_LOGI("当前状态: " + stateMachine->GetCurrentState());

  //测试场景4: 触发事件，条件满足
  SMF_LOGI("=== 测试场景4: 触发事件，条件满足 ===");

  stateMachine->SetConditionValue("pressure", 20);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  stateMachine->HandleEvent(event2);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  SMF_LOGI("当前状态: " + stateMachine->GetCurrentState());

  stateMachine->SetConditionValue("temperature", 20);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  SMF_LOGI("当前状态: " + stateMachine->GetCurrentState());

  // 停止状态机
  stateMachine->Stop();

  SMF_LOGI("=== 测试完成 ===");
  return 0;
}