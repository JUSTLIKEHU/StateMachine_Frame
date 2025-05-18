/**
 * @file state_timeout_test.cpp
 * @brief Test for state timeout mechanism
 * @author xiaokui.huang
 * @date 2025-05-07
 */

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "logger.h"
#include "state_machine.h"
#include "state_machine_factory.h"
using namespace std;
using namespace smf;

class StateTimeoutTester {
 public:
  StateTimeoutTester() : current_state_("UNKNOWN"), transition_count_(0), timeout_count_(0) {
    // 初始化日志系统
    SMF_LOGGER_INIT(LogLevel::DEBUG);
    SMF_LOGGER_SET_FILE("state_timeout_test.log");
  }

  void Run() {
    SMF_LOGI("====== 状态超时测试开始 ======");

    auto fsm = smf::StateMachineFactory::CreateStateMachine("state_timeout_test");

    // 注册回调
    fsm->SetTransitionCallback(this, &StateTimeoutTester::OnTransition);
    fsm->SetEnterStateCallback(this, &StateTimeoutTester::OnEnterState);
    fsm->SetExitStateCallback(this, &StateTimeoutTester::OnExitState);
    fsm->SetPreEventCallback(this, &StateTimeoutTester::OnPreEvent);

    // 初始化状态机
    std::string configPath = "test/state_timeout/config";
    if (!fsm->Init(configPath)) {
      SMF_LOGE("初始化状态机失败");
      return;
    }

    // 启动状态机
    if (!fsm->Start()) {
      SMF_LOGE("启动状态机失败");
      return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 验证初始状态
    std::string current = fsm->GetCurrentState();
    SMF_LOGI("当前状态: " + current);
    if (current != "INIT") {
      SMF_LOGE("初始状态错误，期望: INIT，实际: " + current);
      fsm->Stop();
      return;
    }

    // 触发状态转换
    SMF_LOGI("触发 START 事件...");
    fsm->HandleEvent(std::make_shared<Event>("START"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 验证状态
    current = fsm->GetCurrentState();
    SMF_LOGI("当前状态: " + current);
    if (current != "WORKING") {
      SMF_LOGE("状态错误，期望: WORKING，实际: " + current);
      fsm->Stop();
      return;
    }

    // 触发到等待状态
    SMF_LOGI("触发 WAIT 事件...");
    fsm->HandleEvent(std::make_shared<Event>("WAIT"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 验证状态
    current = fsm->GetCurrentState();
    SMF_LOGI("当前状态: " + current);
    if (current != "WAITING") {
      SMF_LOGE("状态错误，期望: WAITING，实际: " + current);
      fsm->Stop();
      return;
    }

    // 等待超时发生
    SMF_LOGI("等待超时事件触发...");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // 验证超时后的状态
    current = fsm->GetCurrentState();
    SMF_LOGI("超时后的状态: " + current);
    if (current != "COMPLETED") {
      SMF_LOGE("超时后状态错误，期望: COMPLETED，实际: " + current);
      fsm->Stop();
      return;
    }

    // 回到工作状态进行第二次测试
    SMF_LOGI("触发回到 WORKING 状态...");
    fsm->HandleEvent(std::make_shared<Event>("START"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 触发到长等待状态
    SMF_LOGI("触发 LONG_WAIT 事件...");
    fsm->HandleEvent(std::make_shared<Event>("LONG_WAIT"));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 验证状态
    current = fsm->GetCurrentState();
    SMF_LOGI("当前状态: " + current);
    if (current != "LONG_WAIT") {
      SMF_LOGE("状态错误，期望: LONG_WAIT，实际: " + current);
      fsm->Stop();
      return;
    }

    SMF_LOGI("等待12500ms 再停止...");
    std::this_thread::sleep_for(std::chrono::milliseconds(12500));
    fsm->Stop();

    SMF_LOGI("总共发生 " + std::to_string(transition_count_) + " 次状态转换");
    SMF_LOGI("总共发生 " + std::to_string(timeout_count_) + " 次超时事件");
    SMF_LOGI("====== 状态超时测试完成 ======");
  }

 private:
  // 事件预处理回调
  bool OnPreEvent(const State& state, const EventPtr& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检测超时事件
    if (event->GetName() == smf::STATE_TIMEOUT_EVENT) {
      timeout_count_++;
      SMF_LOGI("检测到超时事件！当前状态: " + state);
    }

    // 始终允许事件继续处理
    return true;
  }

  void OnTransition(const std::vector<State>& fromStates, const EventPtr& event,
                    const std::vector<State>& toStates) {
    std::lock_guard<std::mutex> lock(mutex_);
    transition_count_++;

    std::string fromState = fromStates.empty() ? "NONE" : fromStates[0];
    std::string toState = toStates.empty() ? "NONE" : toStates[0];

    SMF_LOGI("状态转换: 从 " + fromState + " 到 " + toState + " 由事件 " + event->GetName() +
             " 触发");
  }

  void OnEnterState(const std::vector<State>& states) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!states.empty()) {
      current_state_ = states[0];
      SMF_LOGI("进入状态: " + current_state_);
    }
  }

  void OnExitState(const std::vector<State>& states) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!states.empty()) {
      SMF_LOGI("退出状态: " + states[0]);
    }
  }

  std::string current_state_;
  int transition_count_;
  int timeout_count_;
  std::mutex mutex_;
};

int main() {
  StateTimeoutTester tester;
  tester.Run();
  return 0;
}