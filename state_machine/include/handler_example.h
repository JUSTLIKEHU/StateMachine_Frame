#include "event.h"
#include "state_machine.h"

namespace smf {

// 创建一个演示如何使用类成员函数作为回调的示例类
class LightController {
 public:
  LightController() : powerOn(false) {}

  // 转换处理
  void HandleTransition(const std::vector<State>& fromStates, const EventPtr& event,
                      const std::vector<State>& toStates) {
    (void)event;  // 防止未使用警告

    // 获取当前状态（层次结构中的第一个）
    State from = fromStates.empty() ? "" : fromStates[0];
    State to = toStates.empty() ? "" : toStates[0];

    if (from == "OFF" && to == "ON") {
      SMF_LOGI("Controller: Light turned ON!");
      powerOn = true;
    } else if (from == "ON" && to == "OFF") {
      SMF_LOGI("Controller: Light turned OFF!");
      powerOn = false;
    }
  }

  // 事件预处理
  bool ValidateEvent(const State& state, const EventPtr& event) {
    SMF_LOGD("Controller: Validating event " + event->toString() + " in state " + state);
    // 例如，仅在灯开启时允许"ADJUST_BRIGHTNESS"事件
    if (*event == "ADJUST_BRIGHTNESS" && state != "ON") {
      SMF_LOGW("Controller: Cannot adjust brightness when light is off!");
      return false;
    }
    return true;
  }

  // 状态进入
  void OnEnter(const std::vector<State>& states) {
    if (!states.empty()) {
      SMF_LOGD("Controller: Entered state " + states[0]);
      if (states[0] == "ON") {
        // 可以执行实际的硬件操作，如通过GPIO打开灯
        SMF_LOGI("Controller: Powering on hardware...");
      }
    }
  }

  // 状态退出
  void OnExit(const std::vector<State>& states) {
    if (!states.empty()) {
      SMF_LOGD("Controller: Exited state " + states[0]);
    }
  }

  // 事件后处理
  void AfterEvent(const EventPtr& event, bool handled) {
    SMF_LOGD("Controller: Processed event " + event->toString() +
             (handled ? " successfully" : " but it was not handled"));
  }

  bool IsPowerOn() const { return powerOn; }

 private:
  bool powerOn;
};

// 更新示例函数，展示如何创建使用类成员函数的处理器
inline std::shared_ptr<StateEventHandler> CreateMemberFunctionHandler() {
  auto controller = std::make_shared<LightController>();
  auto handler = std::make_shared<StateEventHandler>();

  // 绑定类成员函数作为回调
  handler->SetTransitionCallback(controller.get(), &LightController::HandleTransition);
  handler->SetPreEventCallback(controller.get(), &LightController::ValidateEvent);
  handler->SetEnterStateCallback(controller.get(), &LightController::OnEnter);
  handler->SetExitStateCallback(controller.get(), &LightController::OnExit);
  handler->SetPostEventCallback(controller.get(), &LightController::AfterEvent);

  // 注意：这里我们返回了处理器，但要确保controller对象的生命周期必须超过处理器的使用时间
  // 在实际应用中，可能需要让控制器对象的所有权与处理器或状态机的生命周期绑定
  return handler;
}

// 创建一个配置灯光状态处理逻辑的示例函数
inline std::shared_ptr<StateEventHandler> CreateLightStateHandler() {
  auto handler = std::make_shared<StateEventHandler>();

  // 设置转换回调
  handler->SetTransitionCallback([](const std::vector<State>& fromStates, const EventPtr& event,
                                    const std::vector<State>& toStates) {
    (void)event;  // 防止未使用警告

    // 获取当前状态（层次结构中的第一个）
    State from = fromStates.empty() ? "" : fromStates[0];
    State to = toStates.empty() ? "" : toStates[0];

    // 处理逻辑
    if (from == "OFF" && to == "ACTIVE") {
      SMF_LOGI("Light turned ON and is ACTIVE!");
    } else if (from == "ON" && to == "OFF") {
      SMF_LOGI("Light turned OFF!");
    }

    // 打印状态层次
    std::string transition = "Complete transition: ";
    for (const auto& s : fromStates) {
      transition += s + " ";
    }
    transition += "-> ";
    for (const auto& s : toStates) {
      transition += s + " ";
    }
    SMF_LOGD(transition);
  });

  // 设置事件预处理回调
  handler->SetPreEventCallback([](const State& currentState, const EventPtr& event) {
    SMF_LOGD("Pre-processing event: " + event->toString() + " in state: " + currentState);
    if (*event == "unsupported_event") {
      SMF_LOGW("Rejecting unsupported event!");
      return false;
    }
    return true;
  });

  // 设置状态进入回调
  handler->SetEnterStateCallback([](const std::vector<State>& states) {
    if (states.empty())
      return;

    SMF_LOGD("Entering state: " + states[0]);
    if (states[0] == "ON") {
      SMF_LOGI("Turning ON the light!");
    } else if (states[0] == "OFF") {
      SMF_LOGI("Light is now OFF!");
    }
  });

  // 设置状态退出回调
  handler->SetExitStateCallback([](const std::vector<State>& states) {
    if (states.empty())
      return;

    SMF_LOGD("Exiting state: " + states[0]);
    if (states[0] == "ON") {
      SMF_LOGI("Preparing to turn OFF the light...");
    }
  });

  // 设置事件回收回调
  handler->SetPostEventCallback([](const EventPtr& event, bool handled) {
    SMF_LOGD("Post-processing event: " + event->toString() +
             (handled ? " (handled)" : " (not handled)"));
  });

  return handler;
}

}  // namespace smf