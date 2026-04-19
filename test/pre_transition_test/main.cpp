/**
 * @file main.cpp
 * @brief Unit test for the two-phase OnTransition behavior of pending transitions.
 * @details Verifies that:
 *          1) When an event matches but conditions are not yet satisfied
 *             (and timeout > 0), OnTransition is invoked once at pending creation.
 *          2) When the conditions become satisfied later, the pending transition
 *             is consumed: OnExit/SetState/OnEnter run, but OnTransition is NOT
 *             invoked again.
 *          3) When a pending transition expires without the conditions being met,
 *             only the early OnTransition fires; no OnExit/OnEnter occur and the
 *             state remains unchanged.
 *          4) For a regular (non-timeout) transition that is satisfied
 *             immediately, OnTransition/OnExit/OnEnter all fire exactly once.
 * @author xiaokui.hu
 * @date 2026-04-18
 * @version 1.0.0
 */

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "logger.h"
#include "state_machine.h"
#include "state_machine_factory.h"

using namespace smf;

namespace {

struct CallbackCounters {
  std::atomic<int> transitionCount{0};
  std::atomic<int> exitCount{0};
  std::atomic<int> enterCount{0};
  std::atomic<int> postEventCount{0};
  std::string lastTransitionEvent;
  State lastEnterTopState;
  State lastExitTopState;
};

CallbackCounters g_counters;

void OnTransition(const std::vector<State>& exitStates, const EventPtr& event,
                  const std::vector<State>& enterStates) {
  g_counters.transitionCount.fetch_add(1);
  g_counters.lastTransitionEvent = event ? event->GetName() : "";
  std::string from = exitStates.empty() ? "" : exitStates.front();
  std::string to = enterStates.empty() ? "" : enterStates.front();
  SMF_LOGI("[CB] OnTransition #" + std::to_string(g_counters.transitionCount.load()) + ": " +
           from + " -> " + to + " on " + g_counters.lastTransitionEvent);
}

void OnExitState(const std::vector<State>& states) {
  g_counters.exitCount.fetch_add(1);
  if (!states.empty()) {
    g_counters.lastExitTopState = states.front();
  }
  std::string s;
  for (const auto& st : states) s += st + " ";
  SMF_LOGI("[CB] OnExitState  #" + std::to_string(g_counters.exitCount.load()) + ": " + s);
}

void OnEnterState(const std::vector<State>& states) {
  g_counters.enterCount.fetch_add(1);
  if (!states.empty()) {
    g_counters.lastEnterTopState = states.front();
  }
  std::string s;
  for (const auto& st : states) s += st + " ";
  SMF_LOGI("[CB] OnEnterState #" + std::to_string(g_counters.enterCount.load()) + ": " + s);
}

void OnPostEvent(const EventPtr& event, bool handled) {
  g_counters.postEventCount.fetch_add(1);
  SMF_LOGI(std::string("[CB] OnPostEvent: ") + (event ? event->GetName() : "<null>") + " handled=" +
           (handled ? "true" : "false"));
}

#define ASSERT_EQ(actual, expected, msg)                                                       \
  do {                                                                                         \
    auto _a = (actual);                                                                        \
    auto _e = (expected);                                                                      \
    if (!(_a == _e)) {                                                                         \
      std::cerr << "[ASSERT FAILED] " << (msg) << " expected=" << _e << " actual=" << _a       \
                << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl;                    \
      std::exit(1);                                                                            \
    } else {                                                                                   \
      SMF_LOGI(std::string("[ASSERT OK   ] ") + (msg));                                        \
    }                                                                                          \
  } while (0)

}  // namespace

int main() {
  SMF_LOGI("=== Pre-Transition (two-phase OnTransition) Unit Test ===");

  auto sm = StateMachineFactory::CreateStateMachine("PreTransitionTest");
  if (!sm) {
    SMF_LOGE("Failed to create state machine");
    return -1;
  }

  sm->SetTransitionCallback(OnTransition);
  sm->SetEnterStateCallback(OnEnterState);
  sm->SetExitStateCallback(OnExitState);
  sm->SetPostEventCallback(OnPostEvent);

  if (!sm->Init("../../test/pre_transition_test/config")) {
    SMF_LOGE("Failed to init state machine");
    return -1;
  }

  if (!sm->Start()) {
    SMF_LOGE("Failed to start state machine");
    return -1;
  }

  // Allow OnEnterState for the initial state to be processed if any.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  const int baseEnter = g_counters.enterCount.load();
  const int baseExit = g_counters.exitCount.load();
  const int baseTransition = g_counters.transitionCount.load();

  ASSERT_EQ(sm->GetCurrentState(), std::string("idle"), "initial state is idle");

  // ------------------------------------------------------------------
  // Scenario 1: event matches, conditions NOT satisfied, timeout > 0
  //   Expect: OnTransition fires once (pre-transition), state unchanged.
  // ------------------------------------------------------------------
  SMF_LOGI("--- Scenario 1: pre-transition (conditions not satisfied) ---");
  sm->SetConditionValue("temperature", 10);  // unsatisfied (need >=20)
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  sm->HandleEvent(std::make_shared<Event>("go_heat"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  ASSERT_EQ(g_counters.transitionCount.load() - baseTransition, 1,
            "Scenario 1: OnTransition called exactly once at pending creation");
  ASSERT_EQ(g_counters.exitCount.load() - baseExit, 0,
            "Scenario 1: OnExitState NOT called yet");
  ASSERT_EQ(g_counters.enterCount.load() - baseEnter, 0,
            "Scenario 1: OnEnterState NOT called yet");
  ASSERT_EQ(sm->GetCurrentState(), std::string("idle"),
            "Scenario 1: state remains idle");

  // ------------------------------------------------------------------
  // Scenario 2: conditions later become satisfied -> consume pending
  //   Expect: OnExit + OnEnter fire, OnTransition NOT fired again.
  // ------------------------------------------------------------------
  SMF_LOGI("--- Scenario 2: resume pending (conditions now satisfied) ---");
  sm->SetConditionValue("temperature", 25);  // satisfied -> internal event
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  ASSERT_EQ(g_counters.transitionCount.load() - baseTransition, 1,
            "Scenario 2: OnTransition NOT invoked again on resume");
  ASSERT_EQ(g_counters.exitCount.load() - baseExit, 1,
            "Scenario 2: OnExitState invoked exactly once on resume");
  ASSERT_EQ(g_counters.enterCount.load() - baseEnter, 1,
            "Scenario 2: OnEnterState invoked exactly once on resume");
  ASSERT_EQ(sm->GetCurrentState(), std::string("heating"),
            "Scenario 2: state moved to heating");

  // ------------------------------------------------------------------
  // Scenario 3: regular transition, no timeout, condition satisfied immediately
  //   Expect: OnTransition + OnExit + OnEnter all fire once.
  // ------------------------------------------------------------------
  SMF_LOGI("--- Scenario 3: immediate transition (no timeout) ---");
  const int t3 = g_counters.transitionCount.load();
  const int x3 = g_counters.exitCount.load();
  const int e3 = g_counters.enterCount.load();

  sm->SetConditionValue("pressure", 20);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  sm->HandleEvent(std::make_shared<Event>("go_ready"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  ASSERT_EQ(g_counters.transitionCount.load() - t3, 1,
            "Scenario 3: OnTransition invoked once for immediate transition");
  ASSERT_EQ(g_counters.exitCount.load() - x3, 1,
            "Scenario 3: OnExitState invoked once");
  ASSERT_EQ(g_counters.enterCount.load() - e3, 1,
            "Scenario 3: OnEnterState invoked once");
  ASSERT_EQ(sm->GetCurrentState(), std::string("ready"),
            "Scenario 3: state moved to ready");

  // ------------------------------------------------------------------
  // Scenario 4: pending transition expires (conditions never met)
  //   Expect: OnTransition fires once at pending creation, then on expiry
  //           NO Exit/Enter occurs and state remains unchanged.
  // ------------------------------------------------------------------
  SMF_LOGI("--- Scenario 4: pending transition expires without resume ---");
  const int t4 = g_counters.transitionCount.load();
  const int x4 = g_counters.exitCount.load();
  const int e4 = g_counters.enterCount.load();

  sm->SetConditionValue("reset_flag", 0);  // unsatisfied (need ==1)
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  sm->HandleEvent(std::make_shared<Event>("reset_event"));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  ASSERT_EQ(g_counters.transitionCount.load() - t4, 1,
            "Scenario 4: OnTransition invoked once at pending creation");
  ASSERT_EQ(g_counters.exitCount.load() - x4, 0,
            "Scenario 4: no OnExitState yet");
  ASSERT_EQ(g_counters.enterCount.load() - e4, 0,
            "Scenario 4: no OnEnterState yet");

  // Wait long enough for the pending to expire (configured 500ms).
  // Trigger any event so the expiry sweep runs in ProcessEvent.
  std::this_thread::sleep_for(std::chrono::milliseconds(700));
  sm->HandleEvent(std::make_shared<Event>("__noop__"));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  ASSERT_EQ(g_counters.transitionCount.load() - t4, 1,
            "Scenario 4: OnTransition NOT invoked again after expiry");
  ASSERT_EQ(g_counters.exitCount.load() - x4, 0,
            "Scenario 4: still no OnExitState after expiry");
  ASSERT_EQ(g_counters.enterCount.load() - e4, 0,
            "Scenario 4: still no OnEnterState after expiry");
  ASSERT_EQ(sm->GetCurrentState(), std::string("ready"),
            "Scenario 4: state remains ready after expiry");

  // ------------------------------------------------------------------
  // Scenario 5: re-trigger after expiry should create a NEW pending and
  //             invoke OnTransition again (proves the flag is per-pending).
  // ------------------------------------------------------------------
  SMF_LOGI("--- Scenario 5: re-trigger after expiry creates new pending ---");
  const int t5 = g_counters.transitionCount.load();
  const int x5 = g_counters.exitCount.load();
  const int e5 = g_counters.enterCount.load();

  sm->HandleEvent(std::make_shared<Event>("reset_event"));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  ASSERT_EQ(g_counters.transitionCount.load() - t5, 1,
            "Scenario 5: new pending -> OnTransition invoked again");

  sm->SetConditionValue("reset_flag", 1);  // now satisfy
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  ASSERT_EQ(g_counters.transitionCount.load() - t5, 1,
            "Scenario 5: OnTransition NOT invoked on resume of new pending");
  ASSERT_EQ(g_counters.exitCount.load() - x5, 1,
            "Scenario 5: OnExitState invoked on resume");
  ASSERT_EQ(g_counters.enterCount.load() - e5, 1,
            "Scenario 5: OnEnterState invoked on resume");
  ASSERT_EQ(sm->GetCurrentState(), std::string("idle"),
            "Scenario 5: state moved back to idle");

  // ------------------------------------------------------------------
  // Scenario 6: AddPendingTransition dedup
  //   Trigger the SAME event twice while conditions remain unsatisfied.
  //   Expect: only ONE pending entry is created and only ONE OnTransition
  //   pre-callback fires. After conditions are satisfied, the (single)
  //   pending is consumed exactly once.
  //
  //   This guards against the bug where ProcessEvent's FindPendingTransition
  //   branch leaves eventHandled=false, then the FindTransition branch tries
  //   to AddPendingTransition again for the same rule. The dedup inside
  //   TransitionManager::AddPendingTransition must prevent both a duplicate
  //   pending entry and a duplicate OnTransition callback.
  // ------------------------------------------------------------------
  SMF_LOGI("--- Scenario 6: AddPendingTransition dedup (no duplicate OnTransition) ---");
  ASSERT_EQ(sm->GetCurrentState(), std::string("idle"),
            "Scenario 6: precondition - state is idle");

  // Make sure temperature condition is unsatisfied for idle->heating.
  sm->SetConditionValue("temperature", 5);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  const int t6 = g_counters.transitionCount.load();
  const int x6 = g_counters.exitCount.load();
  const int e6 = g_counters.enterCount.load();

  // First trigger: creates pending + fires OnTransition once.
  sm->HandleEvent(std::make_shared<Event>("go_heat"));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  ASSERT_EQ(g_counters.transitionCount.load() - t6, 1,
            "Scenario 6: first trigger -> OnTransition invoked once (pending created)");
  ASSERT_EQ(g_counters.exitCount.load() - x6, 0,
            "Scenario 6: first trigger -> no OnExitState");
  ASSERT_EQ(g_counters.enterCount.load() - e6, 0,
            "Scenario 6: first trigger -> no OnEnterState");
  ASSERT_EQ(sm->GetCurrentState(), std::string("idle"),
            "Scenario 6: first trigger -> state remains idle");

  // Second trigger of the SAME event while conditions are still unsatisfied:
  // dedup must prevent both a duplicate pending entry AND a duplicate OnTransition.
  sm->HandleEvent(std::make_shared<Event>("go_heat"));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  ASSERT_EQ(g_counters.transitionCount.load() - t6, 1,
            "Scenario 6: second trigger -> OnTransition NOT invoked again (dedup)");
  ASSERT_EQ(g_counters.exitCount.load() - x6, 0,
            "Scenario 6: second trigger -> still no OnExitState");
  ASSERT_EQ(g_counters.enterCount.load() - e6, 0,
            "Scenario 6: second trigger -> still no OnEnterState");
  ASSERT_EQ(sm->GetCurrentState(), std::string("idle"),
            "Scenario 6: second trigger -> state still idle");

  // Third trigger to make it really obvious that no matter how many times
  // the event arrives, no further OnTransition fires while a pending exists.
  sm->HandleEvent(std::make_shared<Event>("go_heat"));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  ASSERT_EQ(g_counters.transitionCount.load() - t6, 1,
            "Scenario 6: third trigger -> OnTransition still NOT invoked again");

  // Now satisfy the condition. Because dedup kept exactly ONE pending, only
  // ONE pair of OnExit/OnEnter must fire and OnTransition must not fire again.
  sm->SetConditionValue("temperature", 30);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  ASSERT_EQ(g_counters.transitionCount.load() - t6, 1,
            "Scenario 6: resume -> OnTransition NOT invoked again (only the early one counts)");
  ASSERT_EQ(g_counters.exitCount.load() - x6, 1,
            "Scenario 6: resume -> OnExitState invoked exactly once (single pending consumed)");
  ASSERT_EQ(g_counters.enterCount.load() - e6, 1,
            "Scenario 6: resume -> OnEnterState invoked exactly once (single pending consumed)");
  ASSERT_EQ(sm->GetCurrentState(), std::string("heating"),
            "Scenario 6: resume -> state moved to heating");

  // After the pending is consumed, the same rule cannot match anymore from
  // 'heating' state, so further unrelated triggers should leave counters intact.
  sm->HandleEvent(std::make_shared<Event>("go_heat"));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  ASSERT_EQ(g_counters.transitionCount.load() - t6, 1,
            "Scenario 6: post-resume trigger from heating -> no extra OnTransition");
  ASSERT_EQ(g_counters.exitCount.load() - x6, 1,
            "Scenario 6: post-resume trigger from heating -> no extra OnExitState");
  ASSERT_EQ(g_counters.enterCount.load() - e6, 1,
            "Scenario 6: post-resume trigger from heating -> no extra OnEnterState");

  sm->Stop();

  SMF_LOGI("=== All assertions passed ===");
  return 0;
}
