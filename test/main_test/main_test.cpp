#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "event.h"
#include "handler_example.h"
#include "logger.h"  // 添加日志头文件包含

// 演示如何直接使用类成员函数作为回调
void testMemberFunctionCallbacks() {
  smf::FiniteStateMachine fsm;

  auto controller = new smf::LightController();  // 注意：这里只是演示，实际应用中应当使用智能指针

  // 方式1：直接使用setXXXCallback接口
  fsm.SetTransitionCallback(controller, &smf::LightController::HandleTransition);
  fsm.SetPreEventCallback(controller, &smf::LightController::ValidateEvent);
  fsm.SetEnterStateCallback(controller, &smf::LightController::OnEnter);
  fsm.SetExitStateCallback(controller, &smf::LightController::OnExit);
  fsm.SetPostEventCallback(controller, &smf::LightController::AfterEvent);

  // 或者方式2：先创建处理器，然后设置
  /*
  auto handler = std::make_shared<smf::StateEventHandler>();
  handler->SetTransitionCallback(controller, &smf::LightController::HandleTransition);
  handler->SetPreEventCallback(controller, &smf::LightController::ValidateEvent);
  // ... 其他回调设置
  fsm.SetStateEventHandler(handler);
  */

  // 初始化和运行
  fsm.Init("../../config/fsm_config.json");
  fsm.Start();

  // 触发一些事件和条件
  fsm.HandleEvent(std::make_shared<smf::Event>("TURN_ON"));
  fsm.HandleEvent(std::make_shared<smf::Event>("ADJUST_BRIGHTNESS"));  // 这应该会被validateEvent方法处理

  // 停止状态机
  fsm.Stop();

  // 检查控制器状态
  SMF_LOGI("Light power state: " + std::string(controller->IsPowerOn() ? "ON" : "OFF"));

  delete controller;  // 清理资源（实际应用中使用智能指针避免手动释放）
}

void eventThread(smf::FiniteStateMachine& fsm) {
  for (int i = 0; i < 5; ++i) {
    fsm.HandleEvent(std::make_shared<smf::Event>("TURN_ON"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fsm.HandleEvent(std::make_shared<smf::Event>("TURN_OFF"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void conditionThread(smf::FiniteStateMachine& fsm) {
  for (int i = 0; i < 5; ++i) {
    fsm.SetConditionValue("is_powered", 50);
    fsm.SetConditionValue("is_connected", 75);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    fsm.SetConditionValue("is_powered", 150);
    fsm.SetConditionValue("is_connected", 150);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
}

int main() {
  // 初始化日志系统
  SMF_LOGGER_INIT(smf::LogLevel::DEBUG);

  smf::FiniteStateMachine fsm;

  // 选择回调设置方式

  // 选项1: 使用Lambda函数创建的回调
  auto handler = smf::CreateLightStateHandler();
  fsm.SetStateEventHandler(handler);

  // 选项2: 使用类成员函数创建的回调
  // auto handler = smf::CreateMemberFunctionHandler();
  // fsm.SetStateEventHandler(handler);

  // 选项3: 直接测试成员函数回调（单独函数）
  // testMemberFunctionCallbacks();
  // return 0;

  try {
    if (!fsm.Init("../../config/fsm_config.json")) {
      SMF_LOGE("Failed to initialize state machine");
      return 1;
    }

    if (!fsm.Start()) {
      SMF_LOGE("Failed to start state machine");
      return 1;
    }

    SMF_LOGI("Initial state: " + fsm.GetCurrentState());

    // Test OFF -> IDLE transition with duration
    // while (true) {
    //   static int count = 0;
    //   if (++count % 2 == 1) {
    //     fsm.SetConditionValue("is_powered", 1);
    //     SMF_LOGI("Setting is_powered=1...");
    //     std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    //   } else {
    //     fsm.SetConditionValue("is_powered", 0);
    //     SMF_LOGI("Setting is_powered=0...");
    //     std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    //   }
    //   SMF_LOGE("Current state: " + fsm.GetCurrentState());
    // }
    SMF_LOGI("Setting is_powered=1...");
    fsm.SetConditionValue("is_powered", 1);
    SMF_LOGI("Current state: " + fsm.GetCurrentState());

    // 等待1100ms (比配置的1000ms多一点)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    SMF_LOGI("After waiting for duration: " + fsm.GetCurrentState());

    // Test IDLE -> STAND_BY transition
    fsm.SetConditionValue("service_ready", 1);
    fsm.SetConditionValue("is_connected", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After setting service_ready=1 and is_connected=1: " + fsm.GetCurrentState());

    // Test STAND_BY -> ACTIVE transition
    fsm.HandleEvent(std::make_shared<smf::Event>("START"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After START event: " + fsm.GetCurrentState());

    // Test ACTIVE -> PAUSED transition
    fsm.SetConditionValue("is_paused", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After setting is_paused=1: " + fsm.GetCurrentState());

    // Test PAUSED -> ACTIVE transition
    fsm.SetConditionValue("is_paused", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After setting is_paused=0: " + fsm.GetCurrentState());

    // Test ACTIVE -> STAND_BY transition
    fsm.HandleEvent(std::make_shared<smf::Event>("USER_STOP"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After USER_STOP event: " + fsm.GetCurrentState());

    // // Test STAND_BY -> IDLE transition
    // fsm.SetConditionValue("service_ready", 0);
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // SMF_LOGI("After setting service_ready=0: " + fsm.GetCurrentState());

    SMF_LOGI("Final state: " + fsm.GetCurrentState());

    while (true) {
      // Keep the main thread alive to allow the worker threads to execute
      // This is not a recommended practice and is only used here to keep the example simple
      // In a real-world application, you should use a condition variable to wait for a signal
      // from the worker threads before proceeding with any other
      std::this_thread::sleep_for(std::chrono::seconds(1));
      SMF_LOGI("Current state: " + fsm.GetCurrentState());
    }

    fsm.Stop();
  } catch (const std::exception& e) {
    SMF_LOGE("Error: " + std::string(e.what()));
  }

  return 0;
}