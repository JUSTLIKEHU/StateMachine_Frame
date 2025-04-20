#include <chrono>
#include <iostream>
#include <thread>

#include "handler_example.h"
#include "logger.h"  // 添加日志头文件包含

// 演示如何直接使用类成员函数作为回调
void testMemberFunctionCallbacks() {
  smf::FiniteStateMachine fsm;

  auto controller = new smf::LightController();  // 注意：这里只是演示，实际应用中应当使用智能指针

  // 方式1：直接使用setXXXCallback接口
  fsm.setTransitionCallback(controller, &smf::LightController::handleTransition);
  fsm.setPreEventCallback(controller, &smf::LightController::validateEvent);
  fsm.setEnterStateCallback(controller, &smf::LightController::onEnter);
  fsm.setExitStateCallback(controller, &smf::LightController::onExit);
  fsm.setPostEventCallback(controller, &smf::LightController::afterEvent);

  // 或者方式2：先创建处理器，然后设置
  /*
  auto handler = std::make_shared<smf::StateEventHandler>();
  handler->setTransitionCallback(controller, &smf::LightController::handleTransition);
  handler->setPreEventCallback(controller, &smf::LightController::validateEvent);
  // ... 其他回调设置
  fsm.setStateEventHandler(handler);
  */

  // 初始化和运行
  fsm.Init("../../config/fsm_config.json");
  fsm.start();

  // 触发一些事件和条件
  fsm.handleEvent("TURN_ON");
  fsm.handleEvent("ADJUST_BRIGHTNESS");  // 这应该会被validateEvent方法处理

  // 停止状态机
  fsm.stop();

  // 检查控制器状态
  SMF_LOGI("Light power state: " + std::string(controller->isPowerOn() ? "ON" : "OFF"));

  delete controller;  // 清理资源（实际应用中使用智能指针避免手动释放）
}

void eventThread(smf::FiniteStateMachine& fsm) {
  for (int i = 0; i < 5; ++i) {
    fsm.handleEvent("TURN_ON");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fsm.handleEvent("TURN_OFF");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void conditionThread(smf::FiniteStateMachine& fsm) {
  for (int i = 0; i < 5; ++i) {
    fsm.setConditionValue("is_powered", 50);
    fsm.setConditionValue("is_connected", 75);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    fsm.setConditionValue("is_powered", 150);
    fsm.setConditionValue("is_connected", 150);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
}

int main() {
  // 初始化日志系统
  SMF_LOGGER_INIT(smf::LogLevel::DEBUG);

  smf::FiniteStateMachine fsm;

  // 选择回调设置方式

  // 选项1: 使用Lambda函数创建的回调
  auto handler = smf::createLightStateHandler();
  fsm.setStateEventHandler(handler);

  // 选项2: 使用类成员函数创建的回调
  // auto handler = smf::createMemberFunctionHandler();
  // fsm.setStateEventHandler(handler);

  // 选项3: 直接测试成员函数回调（单独函数）
  // testMemberFunctionCallbacks();
  // return 0;

  try {
    if (!fsm.Init("../../config/fsm_config.json")) {
      SMF_LOGE("Failed to initialize state machine");
      return 1;
    }

    if (!fsm.start()) {
      SMF_LOGE("Failed to start state machine");
      return 1;
    }

    SMF_LOGI("Initial state: " + fsm.getCurrentState());

    // Test OFF -> IDLE transition with duration
    // while (true) {
    //   static int count = 0;
    //   if (++count % 2 == 1) {
    //     fsm.setConditionValue("is_powered", 1);
    //     SMF_LOGI("Setting is_powered=1...");
    //     std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    //   } else {
    //     fsm.setConditionValue("is_powered", 0);
    //     SMF_LOGI("Setting is_powered=0...");
    //     std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    //   }
    //   SMF_LOGE("Current state: " + fsm.getCurrentState());
    // }
    SMF_LOGI("Setting is_powered=1...");
    fsm.setConditionValue("is_powered", 1);
    SMF_LOGI("Current state: " + fsm.getCurrentState());

    // 等待1100ms (比配置的1000ms多一点)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    SMF_LOGI("After waiting for duration: " + fsm.getCurrentState());

    // Test IDLE -> STAND_BY transition
    fsm.setConditionValue("service_ready", 1);
    fsm.setConditionValue("is_connected", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After setting service_ready=1 and is_connected=1: " + fsm.getCurrentState());

    // Test STAND_BY -> ACTIVE transition
    fsm.handleEvent("START");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After START event: " + fsm.getCurrentState());

    // Test ACTIVE -> PAUSED transition
    fsm.setConditionValue("is_paused", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After setting is_paused=1: " + fsm.getCurrentState());

    // Test PAUSED -> ACTIVE transition
    fsm.setConditionValue("is_paused", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After setting is_paused=0: " + fsm.getCurrentState());

    // Test ACTIVE -> STAND_BY transition
    fsm.handleEvent("USER_STOP");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SMF_LOGI("After USER_STOP event: " + fsm.getCurrentState());

    // // Test STAND_BY -> IDLE transition
    // fsm.setConditionValue("service_ready", 0);
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // SMF_LOGI("After setting service_ready=0: " + fsm.getCurrentState());

    SMF_LOGI("Final state: " + fsm.getCurrentState());

    while (true) {
      // Keep the main thread alive to allow the worker threads to execute
      // This is not a recommended practice and is only used here to keep the example simple
      // In a real-world application, you should use a condition variable to wait for a signal
      // from the worker threads before proceeding with any other
      std::this_thread::sleep_for(std::chrono::seconds(1));
      SMF_LOGI("Current state: " + fsm.getCurrentState());
    }

    fsm.stop();
  } catch (const std::exception& e) {
    SMF_LOGE("Error: " + std::string(e.what()));
  }

  return 0;
}