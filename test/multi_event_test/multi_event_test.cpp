#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <chrono>

#include "common_define.h"
#include "components/condition_manager.h"
#include "components/config_loader.h"
#include "components/event_handler.h"
#include "components/state_manager.h"
#include "components/transition_manager.h"
#include "event.h"
#include "logger.h"
#include "state_event_handler.h"
#include "state_machine.h"
#include "state_machine_factory.h"

using namespace smf;

class MultiEventTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 创建测试配置目录
    configDir = "test/multi_event_test/config";
    createTestConfigs();

    // 创建状态机实例
    fsm_ = StateMachineFactory::CreateStateMachine("multi_event_test");

    // 设置状态事件处理器回调
    fsm_->SetTransitionCallback(
        [this](const std::vector<State>& fromStates, const EventPtr& event,
               const std::vector<State>& toStates) {
          State from = fromStates.empty() ? "" : fromStates[0];
          State to = toStates.empty() ? "" : toStates[0];
          lastTransition = {from, to};
          SMF_LOGI("State transition: " + from + " -> " + to + " on event " +
                   event->GetName());
        });
  }

  void TearDown() override {
    // 清理测试配置目录
    std::filesystem::remove_all(configDir);
  }

  bool createTestConfigs() {
    // 创建配置目录结构
    std::filesystem::create_directories(configDir + "/trans_config");
    std::filesystem::create_directories(configDir + "/event_generate_config");

    // 创建状态配置文件
    std::string stateConfig = R"({
      "states": [
        {"name": "ACTIVE"},
        {"name": "STAND_BY"},
        {"name": "OFF"}
      ],
      "initial_state": "OFF"
    })";

    std::string stateFile = configDir + "/state_config.json";
    std::ofstream sf(stateFile);
    if (!sf.is_open()) {
      SMF_LOGE("Failed to create state config file: " + stateFile);
      return false;
    }
    sf << stateConfig;
    sf.close();

    // 创建转换规则配置文件
    std::string transConfig = R"({
      "from": "ACTIVE",
      "to": "STAND_BY",
      "event": ["USER_STOP", "SERVICE_STOP"],
      "conditions": [
        {
          "name": "system_status",
          "range": [0, 0]
        }
      ],
      "conditions_operator": "AND"
    })";

    std::string transFile = configDir + "/trans_config/active_to_standby.json";
    std::ofstream tf(transFile);
    if (!tf.is_open()) {
      SMF_LOGE("Failed to create transition config file: " + transFile);
      return false;
    }
    tf << transConfig;
    tf.close();

    // 创建从 OFF 到 ACTIVE 的转换规则
    std::string offToActiveConfig = R"({
      "from": "OFF",
      "to": "ACTIVE",
      "event": "POWER_ON"
    })";

    std::string offToActiveFile = configDir + "/trans_config/off_to_active.json";
    std::ofstream oaf(offToActiveFile);
    if (!oaf.is_open()) {
      SMF_LOGE("Failed to create transition config file: " + offToActiveFile);
      return false;
    }
    oaf << offToActiveConfig;
    oaf.close();

    // 创建从 STAND_BY 到 ACTIVE 的转换规则
    std::string standbyToActiveConfig = R"({
      "from": "STAND_BY",
      "to": "ACTIVE",
      "event": "RESUME"
    })";

    std::string standbyToActiveFile = configDir + "/trans_config/standby_to_active.json";
    std::ofstream saf(standbyToActiveFile);
    if (!saf.is_open()) {
      SMF_LOGE("Failed to create transition config file: " + standbyToActiveFile);
      return false;
    }
    saf << standbyToActiveConfig;
    saf.close();

    return true;
  }

  std::string configDir;
  std::shared_ptr<FiniteStateMachine> fsm_;
  std::pair<State, State> lastTransition;
};

TEST_F(MultiEventTest, MultipleEventsTransition) {
  // 初始化状态机
  ASSERT_TRUE(fsm_->Init(configDir));
  ASSERT_TRUE(fsm_->Start());

  // 设置初始条件
  fsm_->SetConditionValue("system_status", 1);
  EXPECT_EQ(fsm_->GetCurrentState(), "OFF");

  // 切换到 ACTIVE 状态
  auto event = std::make_shared<Event>("POWER_ON");
  fsm_->HandleEvent(event);
  
  // 等待状态转换完成
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(fsm_->GetCurrentState(), "ACTIVE");

  // 设置条件满足转换要求
  fsm_->SetConditionValue("system_status", 0);

  // 测试第一个触发事件
  event = std::make_shared<Event>("USER_STOP");
  fsm_->HandleEvent(event);
  
  // 等待状态转换完成
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(fsm_->GetCurrentState(), "STAND_BY");

  // 切换回 ACTIVE 状态
  event = std::make_shared<Event>("RESUME");
  fsm_->HandleEvent(event);
  
  // 等待状态转换完成
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(fsm_->GetCurrentState(), "ACTIVE");

  // 测试第二个触发事件
  event = std::make_shared<Event>("SERVICE_STOP");
  fsm_->HandleEvent(event);
  
  // 等待状态转换完成
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(fsm_->GetCurrentState(), "STAND_BY");
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
} 