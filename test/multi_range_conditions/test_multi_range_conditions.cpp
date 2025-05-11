/**
 * @file test_multi_range_conditions.cpp
 * @brief 测试二维数组条件范围功能
 * @author xiaokui.hu
 * @date 2025-04-30
 */

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "logger.h"
#include "state_machine.h"
#include "state_machine_factory.h"
using namespace smf;
using json = nlohmann::json;

// 创建临时配置文件目录和文件
void createTestConfigs() {
  // 确保目录存在
  std::filesystem::create_directories("./test_config");
  std::filesystem::create_directories("./test_config/event_generate_config");
  std::filesystem::create_directories("./test_config/trans_config");

  // 状态配置
  json stateConfig;
  stateConfig["states"] = json::array();
  stateConfig["states"].push_back({{"name", "Idle"}, {"parent", ""}});
  stateConfig["states"].push_back({{"name", "Running"}, {"parent", ""}});
  stateConfig["states"].push_back({{"name", "Error"}, {"parent", ""}});
  stateConfig["initial_state"] = "Idle";

  std::ofstream stateFile("./test_config/state_config.json");
  stateFile << stateConfig.dump(2);
  stateFile.close();

  // 事件生成配置 - 测试一维条件范围
  json event1Config;
  event1Config["name"] = "SimpleRangeEvent";
  event1Config["trigger_mode"] = "edge";
  event1Config["conditions_operator"] = "AND";
  event1Config["conditions"] = json::array();
  event1Config["conditions"].push_back(
      {{"name", "temperature"}, {"range", {30, 50}}, {"duration", 0}});
  std::ofstream event1File("./test_config/event_generate_config/simple_range_event.json");
  event1File << event1Config.dump(2);
  event1File.close();

  // 事件生成配置 - 测试二维条件范围
  json event2Config;
  event2Config["name"] = "MultiRangeEvent";
  event2Config["trigger_mode"] = "edge";
  event2Config["conditions_operator"] = "AND";
  event2Config["conditions"] = json::array();

  // 添加多区间条件，温度在10-20度或者30-40度时触发
  json multiRangeCondition;
  multiRangeCondition["name"] = "multi_temperature";
  multiRangeCondition["range"] = json::array();
  multiRangeCondition["range"].push_back({10, 20});
  multiRangeCondition["range"].push_back({30, 40});
  multiRangeCondition["duration"] = 0;
  event2Config["conditions"].push_back(multiRangeCondition);

  std::ofstream event2File("./test_config/event_generate_config/multi_range_event.json");
  event2File << event2Config.dump(2);
  event2File.close();

  // 状态转移配置 - Idle到Running (简单条件)
  json trans1Config;
  trans1Config["from"] = "Idle";
  trans1Config["to"] = "Running";
  trans1Config["event"] = "SimpleRangeEvent";
  trans1Config["conditions_operator"] = "AND";
  trans1Config["conditions"] = json::array();

  std::ofstream trans1File("./test_config/trans_config/idle_to_running.json");
  trans1File << trans1Config.dump(2);
  trans1File.close();

  // 状态转移配置 - Running到Error (多区间条件)
  json trans2Config;
  trans2Config["from"] = "Running";
  trans2Config["to"] = "Error";
  trans2Config["event"] = "MultiRangeEvent";
  trans2Config["conditions_operator"] = "AND";
  trans2Config["conditions"] = json::array();

  std::ofstream trans2File("./test_config/trans_config/running_to_error.json");
  trans2File << trans2Config.dump(2);
  trans2File.close();

  // 状态转移配置 - Error到Idle (内部事件)
  json trans3Config;
  trans3Config["from"] = "Error";
  trans3Config["to"] = "Idle";
  trans3Config["event"] = "";  // 内部事件
  trans3Config["conditions_operator"] = "AND";
  trans3Config["conditions"] = json::array();
  json resetCondition;
  resetCondition["name"] = "reset";
  resetCondition["range"] = {{1, 1}};  // 简单条件，值为1时触发
  resetCondition["duration"] = 0;
  trans3Config["conditions"].push_back(resetCondition);

  std::ofstream trans3File("./test_config/trans_config/error_to_idle.json");
  trans3File << trans3Config.dump(2);
  trans3File.close();
}

// 状态转换回调
void onTransition(const std::vector<State>& exitStates, const EventPtr& event,
                  const std::vector<State>& enterStates) {
  std::cout << "状态转换: 从 [";
  for (const auto& state : exitStates) {
    std::cout << state << " ";
  }
  std::cout << "] 到 [";
  for (const auto& state : enterStates) {
    std::cout << state << " ";
  }
  std::cout << "] 由事件触发: " << event->toString() << std::endl;
}

int main() {
  // 创建测试配置
  createTestConfigs();

  // 初始化状态机
  auto fsm = smf::StateMachineFactory::CreateStateMachine("test_multi_range_conditions");
  if (!fsm->Init("./test_config")) {
    std::cerr << "初始化状态机失败!" << std::endl;
    return 1;
  }

  // 设置状态转换回调
  fsm->SetTransitionCallback(onTransition);

  // 启动状态机
  if (!fsm->Start()) {
    std::cerr << "启动状态机失败!" << std::endl;
    return 1;
  }

  std::cout << "测试开始，初始状态: " << fsm->GetCurrentState() << std::endl;

  // 测试一维范围条件
  std::cout << "\n===== 测试一维范围条件 =====\n";
  std::cout << "设置温度为40度 (在30-50范围内)" << std::endl;
  fsm->SetConditionValue("temperature", 40);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "当前状态: " << fsm->GetCurrentState() << std::endl;

  // 测试二维范围条件
  std::cout << "\n===== 测试二维范围条件 =====\n";

  // 测试第一个范围区间 10-20
  std::cout << "设置multi_temperature为15度 (在[10,20]范围内)" << std::endl;
  fsm->SetConditionValue("multi_temperature", 15);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "当前状态: " << fsm->GetCurrentState() << std::endl;

  // 测试范围之外的值
  std::cout << "\n设置multi_temperature为25度 (不在任何配置范围内)" << std::endl;
  fsm->SetConditionValue("multi_temperature", 25);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "当前状态: " << fsm->GetCurrentState() << std::endl;

  // 测试第二个范围区间 30-40
  std::cout << "\n设置multi_temperature为35度 (在[30,40]范围内)" << std::endl;
  fsm->SetConditionValue("multi_temperature", 35);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "当前状态: " << fsm->GetCurrentState() << std::endl;

  // 重置回Idle状态
  std::cout << "\n===== 测试重置 =====\n";
  std::cout << "设置reset为1，触发回到Idle状态" << std::endl;
  fsm->SetConditionValue("reset", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "当前状态: " << fsm->GetCurrentState() << std::endl;

  // 停止状态机
  fsm->Stop();
  std::cout << "\n测试完成!" << std::endl;

  return 0;
}