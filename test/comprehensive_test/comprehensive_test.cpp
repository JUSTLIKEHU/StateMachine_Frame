/**
 * @file comprehensive_test.cpp
 * @brief 全面测试状态机框架的所有功能特性
 * @date 2025-01-30
 **/

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <filesystem>

#include "event.h"
#include "logger.h"  // 添加对日志头文件的包含
#include "state_machine.h"
#include "state_machine_factory.h"
// 智能家居系统控制器类
class SmartHomeController {
 public:
  SmartHomeController()
      : powerLevel(0),
        networkConnected(false),
        securityEnabled(false),
        temperatureControlEnabled(false),
        lightingControlEnabled(false) {
    // 初始化日志标题
    SMF_LOGI("===========================================");
    SMF_LOGI("    智能家居状态机系统 - 全面功能测试");
    SMF_LOGI("===========================================");
  }

  // 状态转移回调 - 添加对状态层次的理解
  void onTransition(const std::vector<smf::State>& fromStates, const smf::EventPtr& event,
                    const std::vector<smf::State>& toStates) {
    printStateTransition(fromStates, *event, toStates);

    // 分析状态转换的实际意义
    bool isLeavingOnline = isReallyLeaving(fromStates, toStates, "ONLINE");
    // bool isLeavingPower = isReallyLeaving(fromStates, toStates, "POWER_ON");

    // 第一个元素是最具体的状态
    smf::State specificFrom = fromStates.empty() ? "" : fromStates[0];
    smf::State specificTo = toStates.empty() ? "" : toStates[0];

    // 处理特定状态转换的业务逻辑
    if (specificTo == "STANDBY" && specificFrom != "STANDBY") {
      SMF_LOGI("系统进入待机模式：低功耗运行，保持核心功能...");

      // 如果是从ONLINE状态转到STANDBY，则是网络断开，但电源仍在
      if (isLeavingOnline) {
        networkConnected = false;
        SMF_LOGI("网络连接已断开，系统转为本地功能模式");
      }
    } else if (specificTo == "FULLY_OPERATIONAL" && specificFrom != "FULLY_OPERATIONAL") {
      SMF_LOGI("系统完全运行模式：所有功能模块已启动...");
    } else if (specificTo == "SECURITY_MODE" && specificFrom != "SECURITY_MODE") {
      SMF_LOGI("安全防护模式已激活：监控系统已开启，发送安全警报...");
      securityEnabled = true;
    } else if (specificTo == "ENERGY_SAVING" && specificFrom != "ENERGY_SAVING") {
      SMF_LOGI("节能模式已激活：非必要设备已关闭，能源消耗已最小化...");
      powerLevel = std::max(powerLevel - 30, 0);  // 降低能耗
    } else if (specificTo == "OFF" && specificFrom != "OFF") {
      SMF_LOGI("系统已完全关闭：所有功能已停止，进入零功耗状态...");
      powerLevel = 0;
      networkConnected = false;
      securityEnabled = false;
      temperatureControlEnabled = false;
      lightingControlEnabled = false;
    }
  }

  // 事件预处理回调
  bool onPreEvent(const smf::State& currentState, const smf::EventPtr& event) {
    SMF_LOGI("事件检验: [" + event->toString() + "] 在状态 [" + currentState + "]");

    // 检查特定状态下的事件是否合法
    if (*event == "ACTIVATE_SECURITY" && currentState == "OFF") {
      SMF_LOGW("拒绝处理：系统关闭状态下无法激活安全模式");
      return false;
    } else if (*event == "ENTER_ENERGY_SAVING" && currentState == "SECURITY_MODE") {
      SMF_LOGW("拒绝处理：安全模式下不允许进入节能模式");
      return false;
    }

    return true;
  }

  // 进入状态回调 - 修改为支持层次状态理解
  void onEnterState(const std::vector<smf::State>& states) {
    if (states.empty())
      return;

    std::stringstream ss;
    ss << "进入状态层次: ";
    for (const auto& state : states) {
      ss << "[" << state << "] ";
    }
    SMF_LOGI(ss.str());

    // 逐一处理每个状态层次的进入逻辑
    for (const auto& state : states) {
      handleEnterSpecificState(state);
    }
  }

  // 退出状态回调 - 修改为支持层次状态理解
  void onExitState(const std::vector<smf::State>& states) {
    if (states.empty())
      return;

    std::stringstream ss;
    ss << "退出状态层次: ";
    for (const auto& state : states) {
      ss << "[" << state << "] ";
    }
    SMF_LOGI(ss.str());

    // 检查是否真正离开了某个父状态
    // 例如：从FULLY_OPERATIONAL到SECURITY_MODE，仍然在ONLINE下，不应该关闭网络

    // 逐一处理每个状态层次的退出逻辑
    // 注意：退出逻辑应该按照相反的顺序处理，最具体的状态先退出
    for (auto it = states.rbegin(); it != states.rend(); ++it) {
      handleExitSpecificState(*it);
    }
  }

  // 处理特定状态的进入逻辑
  void handleEnterSpecificState(const smf::State& state) {
    if (state == "ONLINE") {
      if (!networkConnected) {  // 只有在网络未连接时才设置
        networkConnected = true;
        SMF_LOGI("网络连接已建立: 可以访问云服务和远程控制");
      }
    } else if (state == "CLIMATE_CONTROL") {
      if (!temperatureControlEnabled) {  // 只在未启用时才设置
        temperatureControlEnabled = true;
        SMF_LOGI("温控系统已激活: 开始监控和调节室内温度");
      }
    } else if (state == "LIGHTING_CONTROL") {
      if (!lightingControlEnabled) {  // 只在未启用时才设置
        lightingControlEnabled = true;
        SMF_LOGI("照明控制已激活: 开始根据环境光调节照明");
      }
    } else if (state == "POWER_ON") {
      if (powerLevel == 0) {  // 只在电源关闭时才设置
        powerLevel = 50;      // 设置默认电源水平
        SMF_LOGI("系统电源已开启: 当前能源水平为 " + std::to_string(powerLevel) + "%");
      }
    } else if (state == "SECURITY_MODE") {
      if (!securityEnabled) {
        securityEnabled = true;
        SMF_LOGI("安全系统已启用: 开始进行安全监控");
      }
    }
  }

  // 处理特定状态的退出逻辑
  void handleExitSpecificState(const smf::State& state) {
    // 检查是否真正需要关闭某个功能
    // 这里需要了解完整的状态转换上下文

    if (state == "ONLINE") {
      // 只有当真正离开ONLINE状态时才断开网络
      // 例如：ONLINE->STANDBY 或 ONLINE->OFF
      networkConnected = false;
      SMF_LOGI("网络连接已断开: 本地功能仍然可用");
    } else if (state == "CLIMATE_CONTROL") {
      temperatureControlEnabled = false;
      SMF_LOGI("温控系统已停用: 温度调节功能已关闭");
    } else if (state == "LIGHTING_CONTROL") {
      lightingControlEnabled = false;
      SMF_LOGI("照明控制已停用: 照明将保持当前状态");
    } else if (state == "POWER_ON") {
      // 只有当真正离开POWER_ON状态时才关闭电源
      SMF_LOGI("系统正在关闭电源: 保存配置并结束所有进程");
    } else if (state == "SECURITY_MODE") {
      securityEnabled = false;
      SMF_LOGI("安全系统已停用: 停止安全监控");
    }
  }

  // 辅助方法：检查是否真正离开了某个状态
  bool isReallyLeaving(const std::vector<smf::State>& fromStates,
                       const std::vector<smf::State>& toStates, const smf::State& stateName) {
    // 检查fromStates是否包含该状态
    bool inFromStates = false;
    for (const auto& state : fromStates) {
      if (state == stateName) {
        inFromStates = true;
        break;
      }
    }

    // 检查toStates是否包含该状态
    bool inToStates = false;
    for (const auto& state : toStates) {
      if (state == stateName) {
        inToStates = true;
        break;
      }
    }

    // 只有当原状态包含但新状态不包含时，才认为真正离开了
    return inFromStates && !inToStates;
  }

  // 事件后处理回调
  void onPostEvent(const smf::EventPtr& event, bool handled) {
    std::string status = handled ? "已成功处理" : "未被处理";
    SMF_LOGI("事件 [" + event->toString() + "] " + status);

    // 事件处理后的特定逻辑
    if (handled && *event == "POWER_INCREASE") {
      powerLevel = std::min(powerLevel + 10, 100);
      SMF_LOGI("能源水平已增加至: " + std::to_string(powerLevel) + "%");
    } else if (handled && *event == "POWER_DECREASE") {
      powerLevel = std::max(powerLevel - 10, 0);
      SMF_LOGI("能源水平已降低至: " + std::to_string(powerLevel) + "%");
    }
  }

  // 增加电源水平
  void increasePower() {
    powerLevel = std::min(powerLevel + 20, 100);
    SMF_LOGI("能源水平已手动增加至: " + std::to_string(powerLevel) + "%");
  }

  // 减少电源水平
  void decreasePower() {
    powerLevel = std::max(powerLevel - 20, 0);
    SMF_LOGI("能源水平已手动降低至: " + std::to_string(powerLevel) + "%");
  }

  // 获取系统状态报告
  std::string getStatusReport() const {
    std::stringstream ss;
    ss << "\n== 智能家居系统状态报告 =="
       << "\n- 电源水平: " << powerLevel << "%"
       << "\n- 网络连接: " << (networkConnected ? "在线" : "离线")
       << "\n- 安全模式: " << (securityEnabled ? "已启用" : "未启用")
       << "\n- 温控系统: " << (temperatureControlEnabled ? "运行中" : "已停止")
       << "\n- 照明控制: " << (lightingControlEnabled ? "运行中" : "已停止")
       << "\n=======================";
    return ss.str();
  }

  // 获取当前能源水平
  int getPowerLevel() const { return powerLevel; }

  // 获取网络连接状态
  bool isNetworkConnected() const { return networkConnected; }

 private:
  // 打印状态转换
  void printStateTransition(const std::vector<smf::State>& fromStates, const smf::Event& event,
                            const std::vector<smf::State>& toStates) {
    SMF_LOGI("\n------------------------------------------");
    std::stringstream ss;
    ss << "状态转换: ";

    // 打印源状态
    if (fromStates.empty()) {
      ss << "[初始化]";
    } else {
      ss << "[" << fromStates[0] << "]";
    }

    // 打印事件或条件触发
    if (!event.empty()) {
      ss << " -事件-> ";
    } else {
      ss << " -条件-> ";
    }

    // 打印目标状态
    if (toStates.empty()) {
      ss << "[未知]";
    } else {
      ss << "[" << toStates[0] << "]";
    }

    SMF_LOGI(ss.str());

    // 如果有事件，打印事件名称
    if (!event.empty()) {
      SMF_LOGI("触发事件: " + event.toString());
    }
    SMF_LOGI("------------------------------------------");
  }

  // 系统状态变量
  int powerLevel;
  bool networkConnected;
  bool securityEnabled;
  bool temperatureControlEnabled;
  bool lightingControlEnabled;
};

// 为测试创建配置目录和文件
bool createTestConfig(const std::string& configDir) {
  // 创建配置目录
  std::filesystem::create_directories(configDir);
  std::filesystem::create_directories(configDir + "/trans_config");
  std::filesystem::create_directories(configDir + "/event_generate_config");
  
  // 创建状态配置文件
  std::string stateConfigPath = configDir + "/state_config.json";
  std::ofstream stateFile(stateConfigPath);
  if (!stateFile.is_open()) {
    SMF_LOGE("无法创建状态配置文件: " + stateConfigPath);
    return false;
  }

  stateFile << R"({
    "states": [
        {
            "name": "OFF"
        },
        {
            "name": "POWER_ON"
        },
        {
            "name": "STANDBY",
            "parent": "POWER_ON"
        },
        {
            "name": "ONLINE",
            "parent": "POWER_ON"
        },
        {
            "name": "FULLY_OPERATIONAL",
            "parent": "ONLINE"
        },
        {
            "name": "CLIMATE_CONTROL",
            "parent": "FULLY_OPERATIONAL"
        },
        {
            "name": "LIGHTING_CONTROL",
            "parent": "FULLY_OPERATIONAL"
        },
        {
            "name": "SECURITY_MODE",
            "parent": "ONLINE"
        },
        {
            "name": "ENERGY_SAVING",
            "parent": "ONLINE"
        }
    ],
    "initial_state": "OFF"
})";
  stateFile.close();
  
  // 创建转换规则文件
  std::string transitionsConfig[] = {
    // OFF -> STANDBY
    R"({
      "from": "OFF",
      "to": "STANDBY",
      "conditions": [
          {
              "name": "power_level",
              "range": [
                  30,
                  100
              ],
              "duration": 1000
          }
      ],
      "conditions_operator": "AND"
    })",
    
    // POWER_ON -> OFF
    R"({
      "from": "POWER_ON",
      "to": "OFF",
      "conditions": [
          {
              "name": "power_level",
              "range": [
                  0,
                  10
              ]
          }
      ],
      "conditions_operator": "AND"
    })",
    
    // 其他转换规则...
    // 此处省略，但在实际代码中应该包含所有规则
  };
  
  // 写入前两个转换规则作为示例
  for (int i = 0; i < 2; ++i) {
    std::string transFile = configDir + "/trans_config/trans_" + std::to_string(i) + ".json";
    std::ofstream tf(transFile);
    if (!tf.is_open()) {
      SMF_LOGE("无法创建转换规则文件: " + transFile);
      return false;
    }
    tf << transitionsConfig[i];
    tf.close();
  }
  
  SMF_LOGI("成功创建测试配置目录: " + configDir);
  return true;
}

// 运行状态机并执行全面测试
void runComprehensiveTest() {
  SMF_LOGI("\n开始执行全面功能测试...\n");

  // 创建测试配置目录
  std::string configDir = "/tmp/smart_home_config";
  if (!createTestConfig(configDir)) {
    return;
  }

  // 创建状态机
  auto fsm = smf::StateMachineFactory::CreateStateMachine("comprehensive_test");

  // 创建控制器
  auto controller = std::make_shared<SmartHomeController>();

  // 设置回调
  fsm->SetTransitionCallback(controller.get(), &SmartHomeController::onTransition);
  fsm->SetPreEventCallback(controller.get(), &SmartHomeController::onPreEvent);
  fsm->SetEnterStateCallback(controller.get(), &SmartHomeController::onEnterState);
  fsm->SetExitStateCallback(controller.get(), &SmartHomeController::onExitState);
  fsm->SetPostEventCallback(controller.get(), &SmartHomeController::onPostEvent);

  // 初始化并启动状态机
  if (!fsm->Init(configDir + "/state_config.json",
                configDir + "/event_generate_config",
                configDir + "/trans_config")) {
    SMF_LOGE("状态机初始化失败！");
    return;
  }

  if (!fsm->Start()) {
    SMF_LOGE("状态机启动失败！");
    return;
  }

  SMF_LOGI("\n=== 状态机已初始化和启动 ===");
  SMF_LOGI("- 初始状态: " + fsm->GetCurrentState());

  SMF_LOGI("\n=== 测试 1: 条件触发状态转换（带持续时间） ===");
  SMF_LOGI("设置 power_level=50 (需持续1秒)...");
  fsm->SetConditionValue("power_level", 50);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  SMF_LOGI("500ms 后，状态: " + fsm->GetCurrentState());
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  SMF_LOGI("再过 600ms 后，状态: " + fsm->GetCurrentState());

  SMF_LOGI("\n=== 测试 2: 普通条件触发转换 ===");
  SMF_LOGI("设置 network_available=1...");
  fsm->SetConditionValue("network_available", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());
  SMF_LOGI(controller->getStatusReport());

  SMF_LOGI("\n=== 测试 3: 事件触发转换 ===");
  SMF_LOGI("发送事件: START_FULL_OPERATION...");
  fsm->HandleEvent(std::make_shared<smf::Event>("START_FULL_OPERATION"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("\n=== 测试 4: 特定状态下的事件触发 ===");
  SMF_LOGI("发送事件: ACTIVATE_CLIMATE...");
  fsm->HandleEvent(std::make_shared<smf::Event>("ACTIVATE_CLIMATE"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("发送事件: ACTIVATE_LIGHTING (在不同状态层次下)...");
  fsm->HandleEvent(std::make_shared<smf::Event>("ACTIVATE_LIGHTING"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("返回上一级状态...");
  fsm->HandleEvent(std::make_shared<smf::Event>("DEACTIVATE_LIGHTING"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("\n=== 测试 5: 条件变化不满足时的状态保持 ===");
  SMF_LOGI("设置 power_level=5 (但不会立即触发转换)...");
  controller->decreasePower();  // 降低电源水平
  fsm->SetConditionValue("power_level", controller->getPowerLevel());
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());
  SMF_LOGI(controller->getStatusReport());

  SMF_LOGI("\n=== 测试 6: 事件拒绝处理逻辑 ===");
  SMF_LOGI("先关闭系统...");
  fsm->SetConditionValue("power_level", 0);  // 设置电源为0
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("在OFF状态下发送ACTIVATE_SECURITY事件 (应被拒绝)...");
  fsm->HandleEvent(std::make_shared<smf::Event>("ACTIVATE_SECURITY"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("\n=== 测试 7: 恢复系统并测试安全模式 ===");
  SMF_LOGI("重新设置 power_level=60...");
  controller->increasePower();  // 恢复电源
  fsm->SetConditionValue("power_level", controller->getPowerLevel());
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));  // 等待持续时间条件满足

  SMF_LOGI("设置网络连接...");
  fsm->SetConditionValue("network_available", 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("激活安全模式...");
  fsm->HandleEvent(std::make_shared<smf::Event>("ACTIVATE_SECURITY"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());
  SMF_LOGI(controller->getStatusReport());

  SMF_LOGI("\n=== 测试 8: 在安全模式下测试无效事件 ===");
  SMF_LOGI("尝试进入节能模式 (应被拒绝)...");
  fsm->HandleEvent(std::make_shared<smf::Event>("ENTER_ENERGY_SAVING"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("\n=== 测试 9: 复杂状态序列 ===");
  SMF_LOGI("退出安全模式...");
  fsm->HandleEvent(std::make_shared<smf::Event>("DEACTIVATE_SECURITY"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  SMF_LOGI("进入节能模式...");
  fsm->HandleEvent(std::make_shared<smf::Event>("ENTER_ENERGY_SAVING"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("退出节能模式，进入完全运行状态...");
  fsm->HandleEvent(std::make_shared<smf::Event>("EXIT_ENERGY_SAVING"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  fsm->HandleEvent(std::make_shared<smf::Event>("START_FULL_OPERATION"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  SMF_LOGI("依次激活温控和照明系统...");
  fsm->HandleEvent(std::make_shared<smf::Event>("ACTIVATE_CLIMATE"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  fsm->HandleEvent(std::make_shared<smf::Event>("DEACTIVATE_CLIMATE"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  fsm->HandleEvent(std::make_shared<smf::Event>("ACTIVATE_LIGHTING"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  SMF_LOGI("模拟网络断开...");
  fsm->SetConditionValue("network_available", 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  SMF_LOGI("当前状态: " + fsm->GetCurrentState());

  SMF_LOGI("\n=== 测试 10: 关闭系统 ===");
  SMF_LOGI("将电源水平设置为0...");
  fsm->SetConditionValue("power_level", 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  SMF_LOGI("最终状态: " + fsm->GetCurrentState());
  SMF_LOGI(controller->getStatusReport());

  // 停止状态机
  fsm->Stop();
  SMF_LOGI("\n状态机已停止。全面测试完成！");
}

// 主函数
int main() {
  try {
    // 初始化日志系统
    SMF_LOGGER_INIT(smf::LogLevel::INFO);

    // 执行全面测试
    runComprehensiveTest();
  } catch (const std::exception& e) {
    SMF_LOGE("错误: " + std::string(e.what()));
    return 1;
  }

  return 0;
}
