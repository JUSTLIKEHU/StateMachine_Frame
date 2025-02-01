#include <chrono>
#include <iostream>
#include <thread>

#include "state_machine.h"

void eventThread(FiniteStateMachine& fsm) {
  for (int i = 0; i < 5; ++i) {
    fsm.handleEvent("TURN_ON");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    fsm.handleEvent("TURN_OFF");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void conditionThread(FiniteStateMachine& fsm) {
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
  FiniteStateMachine fsm;
  auto handler = std::make_shared<LightTransitionHandler>();
  fsm.setTransitionHandler(handler);

  try {
    if (!fsm.Init("../../config/fsm_config.json")) {
      std::cerr << "Failed to initialize state machine" << std::endl;
      return 1;
    }

    if (!fsm.start()) {
      std::cerr << "Failed to start state machine" << std::endl;
      return 1;
    }

    std::cout << "Initial state: " << fsm.getCurrentState() << std::endl;

    // Test OFF -> IDLE transition with duration
    std::cout << "Setting is_powered=1..." << std::endl;
    fsm.setConditionValue("is_powered", 1);
    std::cout << "Current state: " << fsm.getCurrentState() << std::endl;

    // 等待1100ms (比配置的1000ms多一点)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    std::cout << "After waiting for duration: " << fsm.getCurrentState() << std::endl;

    // Test IDLE -> STAND_BY transition
    fsm.setConditionValue("service_ready", 1);
    fsm.setConditionValue("is_connected", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "After setting service_ready=1 and is_connected=1: " << fsm.getCurrentState()
              << std::endl;

    // Test STAND_BY -> ACTIVE transition
    fsm.handleEvent("START");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "After START event: " << fsm.getCurrentState() << std::endl;

    // Test ACTIVE -> PAUSED transition
    fsm.setConditionValue("is_paused", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "After setting is_paused=1: " << fsm.getCurrentState() << std::endl;

    // Test PAUSED -> ACTIVE transition
    fsm.setConditionValue("is_paused", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "After setting is_paused=0: " << fsm.getCurrentState() << std::endl;

    // Test ACTIVE -> STAND_BY transition
    fsm.handleEvent("USER_STOP");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "After USER_STOP event: " << fsm.getCurrentState() << std::endl;

    // // Test STAND_BY -> IDLE transition
    // fsm.setConditionValue("service_ready", 0);
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // std::cout << "After setting service_ready=0: " << fsm.getCurrentState() << std::endl;

    std::cout << "Final state: " << fsm.getCurrentState() << std::endl;

    while (true) {
      // Keep the main thread alive to allow the worker threads to execute
      // This is not a recommended practice and is only used here to keep the example simple
      // In a real-world application, you should use a condition variable to wait for a signal
      // from the worker threads before proceeding with any other
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::cout << "Current state: " << fsm.getCurrentState() << std::endl;
    }

    fsm.stop();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  return 0;
}