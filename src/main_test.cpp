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

    // 创建多个线程同时操作状态机
    std::thread t1(eventThread, std::ref(fsm));
    std::thread t2(conditionThread, std::ref(fsm));

    // 等待线程完成
    t1.join();
    t2.join();

    std::cout << "Final state: " << fsm.getCurrentState() << std::endl;

    fsm.stop();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  return 0;
}