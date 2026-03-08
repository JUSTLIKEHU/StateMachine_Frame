/**
 * @file condition_expr_test.cpp
 * @brief Test for complex condition expressions (条件表达式测试)
 * @details 测试复杂自定义条件关系：
 *          1. 支持 [A AND B] 条件组合
 *          2. 支持多组条件 [[A AND B], [B AND C]]，满足任意一组即可
 *          3. 支持取反逻辑 ! ，如 [!A OR !B]
 *          4. 支持按顺序计算：A OR B AND C = (A OR B) AND C
 */

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "logger.h"
#include "state_machine_factory.h"

using namespace smf;

class ConditionExprTestHandler {
 public:
  void OnTransition(const std::vector<State>& fromStates, const EventPtr& event,
                    const std::vector<State>& toStates) {
    std::cout << "[Transition] ";
    if (!fromStates.empty()) {
      std::cout << fromStates[0];
    }
    std::cout << " -> ";
    if (!toStates.empty()) {
      std::cout << toStates[0];
    }
    std::cout << " (event: " << event->GetName() << ")" << std::endl;
  }

  void OnEnterState(const std::vector<State>& states) {
    if (!states.empty()) {
      std::cout << "[EnterState] " << states[0] << std::endl;
    }
  }

  void OnExitState(const std::vector<State>& states) {
    if (!states.empty()) {
      std::cout << "[ExitState] " << states[0] << std::endl;
    }
  }
};

void printCurrentState(const std::shared_ptr<FiniteStateMachine>& fsm) {
  std::cout << ">>> Current State: " << fsm->GetCurrentState() << std::endl;
}

void testCase1_SimpleAndCondition(const std::shared_ptr<FiniteStateMachine>& fsm) {
  std::cout << "\n========== Test Case 1: [A AND B] ==========" << std::endl;
  std::cout << "Testing: INIT -> STATE_A requires [cond_A AND cond_B]" << std::endl;

  printCurrentState(fsm);

  std::cout << "\n--- Setting cond_A = 1 (should NOT transition) ---" << std::endl;
  fsm->SetConditionValue("cond_A", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  printCurrentState(fsm);

  std::cout << "\n--- Setting cond_B = 1 (should transition to STATE_A) ---" << std::endl;
  fsm->SetConditionValue("cond_B", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  printCurrentState(fsm);
}

void testCase2_MultipleConditionGroups(const std::shared_ptr<FiniteStateMachine>& fsm) {
  std::cout << "\n========== Test Case 2: [[A AND B], [B AND C]] ==========" << std::endl;
  std::cout << "Testing: STATE_A -> STATE_B requires [[cond_A AND cond_B] OR [cond_B AND cond_C]]"
            << std::endl;

  printCurrentState(fsm);

  // 重置条件
  fsm->SetConditionValue("cond_A", 0);
  fsm->SetConditionValue("cond_B", 0);
  fsm->SetConditionValue("cond_C", 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "\n--- Setting cond_B = 1, cond_C = 1 (should transition via [B AND C]) ---"
            << std::endl;
  fsm->SetConditionValue("cond_B", 1);
  fsm->SetConditionValue("cond_C", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  printCurrentState(fsm);
}

void testCase3_NegationLogic(const std::shared_ptr<FiniteStateMachine>& fsm) {
  std::cout << "\n========== Test Case 3: [!A OR !B] ==========" << std::endl;
  std::cout << "Testing: STATE_B -> STATE_C requires [!cond_A OR !cond_B]" << std::endl;

  printCurrentState(fsm);

  std::cout << "\n--- Current: cond_A=0, cond_B=1 (should transition: !A is true) ---" << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  printCurrentState(fsm);
}

void testCase4_SequentialOperators(const std::shared_ptr<FiniteStateMachine>& fsm) {
  std::cout << "\n========== Test Case 4: A OR B AND C ==========" << std::endl;
  std::cout << "Testing: STATE_C -> STATE_D requires [cond_A OR cond_B AND cond_C]" << std::endl;
  std::cout << "This means: (cond_A OR cond_B) AND cond_C" << std::endl;

  printCurrentState(fsm);

  // 重置条件
  fsm->SetConditionValue("cond_A", 0);
  fsm->SetConditionValue("cond_B", 0);
  fsm->SetConditionValue("cond_C", 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::cout << "\n--- Setting cond_A=1 only (should NOT transition: need C too) ---" << std::endl;
  fsm->SetConditionValue("cond_A", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  printCurrentState(fsm);

  std::cout << "\n--- Setting cond_C=1 (should transition: (A OR B) AND C is now true) ---"
            << std::endl;
  fsm->SetConditionValue("cond_C", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  printCurrentState(fsm);
}

int main() {
  SMF_LOGGER_INIT(LogLevel::DEBUG);

  std::cout << "============================================" << std::endl;
  std::cout << "   Complex Condition Expression Test" << std::endl;
  std::cout << "============================================" << std::endl;

  auto fsm = StateMachineFactory::CreateStateMachine("condition_expr_test");

  auto handler = std::make_shared<ConditionExprTestHandler>();
  fsm->SetTransitionCallback(handler.get(), &ConditionExprTestHandler::OnTransition);
  fsm->SetEnterStateCallback(handler.get(), &ConditionExprTestHandler::OnEnterState);
  fsm->SetExitStateCallback(handler.get(), &ConditionExprTestHandler::OnExitState);

  std::string configDir = "../../test/condition_expr_test/config";
  if (!fsm->Init(configDir)) {
    std::cerr << "Failed to initialize state machine!" << std::endl;
    return 1;
  }

  fsm->Start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  testCase1_SimpleAndCondition(fsm);
  testCase2_MultipleConditionGroups(fsm);
  testCase3_NegationLogic(fsm);
  testCase4_SequentialOperators(fsm);

  std::cout << "\n============================================" << std::endl;
  std::cout << "   Test Completed!" << std::endl;
  std::cout << "============================================" << std::endl;

  fsm->Stop();

  return 0;
}
