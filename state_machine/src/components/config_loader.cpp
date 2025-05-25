/**
 * @file config_loader.cpp
 * @brief Implementation of configuration loader component
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file contains the implementation of the ConfigLoader class,
 *          which is responsible for loading state machine configuration from
 *          JSON files, including states, events, and transitions.
 * @version 1.0.0
 **/

/**
 * MIT License
 *
 * Copyright (c) 2025 xiaokui.hu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "components/config_loader.h"

#include <chrono>
#include <fstream>
#include <thread>

#include "common_define.h"
#include "components/i_condition_manager.h"
#include "components/i_event_handler.h"
#include "components/i_state_manager.h"
#include "components/i_transition_manager.h"
#include "logger.h"

namespace smf {

ConfigLoader::ConfigLoader(IStateManager* state_manager, IConditionManager* condition_manager,
                           ITransitionManager* transition_manager, IEventHandler* event_handler)
    : state_manager_(state_manager),
      condition_manager_(condition_manager),
      transition_manager_(transition_manager),
      event_handler_(event_handler) {}

ConfigLoader::~ConfigLoader() { Stop(); }

void ConfigLoader::Start() {
  bool expected = false;
  if (running_.compare_exchange_strong(expected, true)) {
    SMF_LOGI("ConfigLoader started");
  }
}

void ConfigLoader::Stop() {
  bool expected = true;
  if (running_.compare_exchange_strong(expected, false)) {
    SMF_LOGI("ConfigLoader stopped");
  }
}

bool ConfigLoader::IsRunning() const { return running_; }

bool ConfigLoader::LoadConfig(const std::string& configFile) {
  if (running_) {
    SMF_LOGE("ConfigLoader is running cannot load config");
    return false;
  }

  std::filesystem::path path(configFile);
  if (std::filesystem::is_regular_file(path)) {
    // 如果是状态配置文件，则提取目录部分作为配置根目录
    std::string configDir = path.parent_path().string();
    std::string eventConfigDir = configDir + "/event_generate_config";
    std::string transConfigDir = configDir + "/trans_config";

    return LoadStateConfig(configFile) && LoadEventConfig(eventConfigDir) &&
           LoadTransitionConfig(transConfigDir);
  } else if (std::filesystem::is_directory(path)) {
    // 如果是目录，按照原有逻辑使用子目录
    std::string stateConfigFile = configFile + "/state_config.json";
    std::string eventConfigDir = configFile + "/event_generate_config";
    std::string transConfigDir = configFile + "/trans_config";

    return LoadStateConfig(stateConfigFile) && LoadEventConfig(eventConfigDir) &&
           LoadTransitionConfig(transConfigDir);
  }
  SMF_LOGE("Invalid config path: " + configFile);
  return false;
}

bool ConfigLoader::LoadStateConfig(const std::string& stateConfigFile) {
  if (running_) {
    SMF_LOGE("ConfigLoader is running cannot load config");
    return false;
  }

  json config;
  if (!LoadJsonFile(stateConfigFile, config)) {
    return false;
  }

  if (!ValidateStateConfig(config)) {
    return false;
  }

  return ParseStateConfig(config);
}

bool ConfigLoader::LoadEventConfig(const std::string& eventConfigDir) {
  if (running_) {
    SMF_LOGE("ConfigLoader is running cannot load config");
    return false;
  }

  auto configFiles = GetJsonFilesInDirectory(eventConfigDir);
  if (configFiles.empty()) {
    SMF_LOGW("No event config files found in: " + eventConfigDir);
    return true;
  }

  bool success = true;
  for (const auto& file : configFiles) {
    json config;
    if (!LoadJsonFile(file, config)) {
      success = false;
      continue;
    }

    if (!ValidateEventConfig(config)) {
      success = false;
      continue;
    }

    if (!ParseEventConfig(config)) {
      success = false;
    }
  }

  return success;
}

bool ConfigLoader::LoadTransitionConfig(const std::string& transConfigDir) {
  if (running_) {
    SMF_LOGE("ConfigLoader is running cannot load config");
    return false;
  }

  auto configFiles = GetJsonFilesInDirectory(transConfigDir);
  if (configFiles.empty()) {
    SMF_LOGE("No transition config files found in: " + transConfigDir);
    return false;
  }

  bool success = true;
  for (const auto& file : configFiles) {
    json config;
    if (!LoadJsonFile(file, config)) {
      success = false;
      continue;
    }

    if (!ValidateTransitionConfig(config)) {
      success = false;
      continue;
    }

    if (!ParseTransitionConfig(config)) {
      success = false;
    }
  }

  return success;
}

bool ConfigLoader::ValidateStateConfig(const json& config) const {
  try {
    if (!config.contains("states") || !config["states"].is_array()) {
      SMF_LOGE("Missing or invalid 'states' array in state config");
      return false;
    }

    if (!config.contains("initial_state") || !config["initial_state"].is_string()) {
      SMF_LOGE("Missing or invalid 'initial_state' in state config");
      return false;
    }

    for (const auto& state : config["states"]) {
      if (!state.contains("name") || !state["name"].is_string()) {
        SMF_LOGE("Missing or invalid 'name' in state definition");
        return false;
      }

      if (state.contains("parent") && !state["parent"].is_string()) {
        SMF_LOGE("Invalid 'parent' in state definition");
        return false;
      }

      if (state.contains("timeout") && !state["timeout"].is_number()) {
        SMF_LOGE("Invalid 'timeout' in state definition");
        return false;
      }
    }

    return true;
  } catch (const json::exception& e) {
    SMF_LOGE("JSON error in state config: " + std::string(e.what()));
    return false;
  }
}

bool ConfigLoader::ValidateEventConfig(const json& config) const {
  try {
    if (!config.contains("name") || !config["name"].is_string()) {
      SMF_LOGE("Missing or invalid 'name' in event config");
      return false;
    }

    if (config.contains("trigger_mode")) {
      if (!config["trigger_mode"].is_string() ||
          (config["trigger_mode"] != "edge" && config["trigger_mode"] != "level")) {
        SMF_LOGE("Invalid 'trigger_mode' in event config");
        return false;
      }
    }

    if (config.contains("conditions")) {
      if (!config["conditions"].is_array()) {
        SMF_LOGE("Invalid 'conditions' array in event config");
        return false;
      }

      for (const auto& condition : config["conditions"]) {
        if (!ValidateCondition(condition)) {
          return false;
        }
      }
    }

    return true;
  } catch (const json::exception& e) {
    SMF_LOGE("JSON error in event config: " + std::string(e.what()));
    return false;
  }
}

bool ConfigLoader::ValidateTransitionConfig(const json& config) const {
  try {
    if (!config.contains("from") || !config["from"].is_string()) {
      SMF_LOGE("Missing or invalid 'from' state in transition config");
      return false;
    }

    if (!config.contains("to") || !config["to"].is_string()) {
      SMF_LOGE("Missing or invalid 'to' state in transition config");
      return false;
    }

    if (config.contains("event")) {
      if (!config["event"].is_string() && !config["event"].is_array()) {
        SMF_LOGE("Invalid 'event' in transition config - must be string or array");
        return false;
      }
      if (config["event"].is_array()) {
        for (const auto& event : config["event"]) {
          if (!event.is_string()) {
            SMF_LOGE("Invalid event in array - all events must be strings");
            return false;
          }
        }
      }
    }

    if (config.contains("conditions")) {
      if (!config["conditions"].is_array()) {
        SMF_LOGE("Invalid 'conditions' array in transition config");
        return false;
      }

      for (const auto& condition : config["conditions"]) {
        if (!ValidateCondition(condition)) {
          return false;
        }
      }
    }

    return true;
  } catch (const json::exception& e) {
    SMF_LOGE("JSON error in transition config: " + std::string(e.what()));
    return false;
  }
}

bool ConfigLoader::ParseStateConfig(const json& config) {
  try {
    // 先添加所有状态
    for (const auto& state : config["states"]) {
      std::string name = state["name"];
      std::string parent = state.value("parent", "");
      int timeout = state.value("timeout", 0);
      state_names_.emplace(name);
      StateInfo stateInfo{name, parent, {}, timeout};
      if (!state_manager_->AddStateInfo(stateInfo)) {
        SMF_LOGE("Failed to add state: " + name);
        return false;
      }
    }

    // 设置初始状态
    std::string initialState = config["initial_state"];
    if (!state_manager_->SetState(initialState)) {
      SMF_LOGE("Failed to set initial state: " + initialState);
      return false;
    }

    return true;
  } catch (const json::exception& e) {
    SMF_LOGE("Error parsing state config: " + std::string(e.what()));
    return false;
  }
}

bool ConfigLoader::ParseEventConfig(const json& config) {
  try {
    EventDefinition eventDef;
    eventDef.name = config["name"];
    eventDef.trigger_mode = config.value("trigger_mode", "edge");
    eventDef.conditionsOperator = config.value("conditions_operator", "AND");

    if (config.contains("conditions")) {
      for (const auto& condJson : config["conditions"]) {
        Condition cond;
        cond.name = condJson["name"];
        cond.duration = condJson.value("duration", 0);

        if (condJson.contains("range")) {
          if (condJson["range"].is_array()) {
            if (condJson["range"].size() == 2 && condJson["range"][0].is_number() &&
                condJson["range"][1].is_number()) {
              // 简单的一维数组格式 [min, max]
              cond.range_values.push_back({condJson["range"][0], condJson["range"][1]});
            } else {
              // 二维数组格式 [[min1, max1], [min2, max2], ...]
              for (const auto& range : condJson["range"]) {
                if (range.is_array() && range.size() == 2) {
                  cond.range_values.push_back({range[0], range[1]});
                } else {
                  SMF_LOGE("Invalid range format in event config: " + eventDef.name);
                  return false;
                }
              }
            }
          } else {
            SMF_LOGE("Range must be an array in event config: " + eventDef.name);
            return false;
          }
        }

        eventDef.conditions.push_back(cond);
        condition_manager_->AddCondition(cond);
      }
    }

    if (!event_handler_->AddEventDefinition(eventDef)) {
      SMF_LOGE("Failed to add event definition: " + eventDef.name);
      return false;
    }

    return true;
  } catch (const json::exception& e) {
    SMF_LOGE("Error parsing event config: " + std::string(e.what()));
    return false;
  }
}

bool ConfigLoader::ParseTransitionConfig(const json& config) {
  try {
    TransitionRule rule;
    rule.from = config["from"];
    rule.to = config["to"];
    
    // 处理事件字段
    if (config.contains("event")) {
      if (config["event"].is_string()) {
        // 单个事件
        rule.events.push_back(config["event"].get<std::string>());
      } else if (config["event"].is_array()) {
        // 多个事件
        for (const auto& event : config["event"]) {
          rule.events.push_back(event.get<std::string>());
        }
      }
    }
    
    if (rule.events.empty()) {
      SMF_LOGW("Missing 'event' in transition config: " + rule.from + " -> " + rule.to +
               " , use INTERNAL_EVENT as default");
      rule.events.push_back(INTERNAL_EVENT);
    }
    
    rule.conditionsOperator = config.value("conditions_operator", "AND");

    if (state_names_.find(rule.from) == state_names_.end() ||
        state_names_.find(rule.to) == state_names_.end()) {
      SMF_LOGE("Invalid 'from' or 'to' state in transition config: " + rule.from + " -> " +
               rule.to);
      return false;
    }

    if (config.contains("conditions")) {
      for (const auto& condJson : config["conditions"]) {
        Condition cond;
        cond.name = condJson["name"];
        cond.duration = condJson.value("duration", 0);

        if (condJson.contains("range")) {
          if (condJson["range"].is_array()) {
            if (condJson["range"].size() == 2 && condJson["range"][0].is_number() &&
                condJson["range"][1].is_number()) {
              // 简单的一维数组格式 [min, max]
              cond.range_values.push_back({condJson["range"][0], condJson["range"][1]});
            } else {
              // 二维数组格式 [[min1, max1], [min2, max2], ...]
              for (const auto& range : condJson["range"]) {
                if (range.is_array() && range.size() == 2) {
                  cond.range_values.push_back({range[0], range[1]});
                } else {
                  SMF_LOGE("Invalid range format in transition config for: " + rule.from);
                  return false;
                }
              }
            }
          } else {
            SMF_LOGE("Range must be an array in transition config for: " + rule.from);
            return false;
          }
        }

        rule.conditions.push_back(cond);
        condition_manager_->AddCondition(cond);
      }
    }

    if (!transition_manager_->AddTransition(rule)) {
      SMF_LOGE("Failed to add transition rule: " + rule.from + " -> " + rule.to);
      return false;
    }

    return true;
  } catch (const json::exception& e) {
    SMF_LOGE("Error parsing transition config: " + std::string(e.what()));
    return false;
  }
}

bool ConfigLoader::LoadJsonFile(const std::string& filePath, json& jsonData) {
  try {
    // 检查文件是否存在
    if (!std::filesystem::exists(filePath)) {
      SMF_LOGE("File not found: " + filePath);
      return false;
    }
    // 读取文件
    std::ifstream file(filePath);
    if (!file.is_open()) {
      SMF_LOGE("Failed to open file: " + filePath);
      return false;
    }

    // 解析JSON
    file >> jsonData;
    return true;
  } catch (const json::exception& e) {
    SMF_LOGE("JSON parsing error in file " + filePath + ": " + std::string(e.what()));
    return false;
  } catch (const std::exception& e) {
    SMF_LOGE("Error reading file " + filePath + ": " + std::string(e.what()));
    return false;
  }
}

std::vector<std::string> ConfigLoader::GetJsonFilesInDirectory(const std::string& dirPath) {
  std::vector<std::string> jsonFiles;

  try {
    if (!std::filesystem::exists(dirPath)) {
      SMF_LOGE("Directory not found: " + dirPath);
      return jsonFiles;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        jsonFiles.push_back(entry.path().string());
      }
    }
  } catch (const std::exception& e) {
    SMF_LOGE("Error reading directory " + dirPath + ": " + std::string(e.what()));
  }

  return jsonFiles;
}

bool ConfigLoader::ValidateCondition(const json& condition) const {
  if (!condition.contains("name") || !condition["name"].is_string()) {
    SMF_LOGE("Missing or invalid 'name' in condition");
    return false;
  }

  if (condition.contains("duration")) {
    if (!condition["duration"].is_number() || condition["duration"].get<int>() < 0) {
      SMF_LOGE("Invalid 'duration' in condition");
      return false;
    }
  }

  if (!condition.contains("range")) {
    SMF_LOGE("Missing 'range' in condition: " + condition["name"].get<std::string>());
    return false;
  }

  if (!condition["range"].is_array()) {
    SMF_LOGE("'range' must be an array in condition: " + condition["name"].get<std::string>());
    return false;
  }

  SMF_LOGI("Validating condition: " + condition["name"].get<std::string>() +
           ", range array size: " + std::to_string(condition["range"].size()));

  // 检查是简单范围还是多范围
  bool isMultiRange = false;
  // 如果第一个元素是数组，则认为是多范围模式
  if (condition["range"].size() > 0 && condition["range"][0].is_array()) {
    isMultiRange = true;
  }

  // 简单范围模式：[min, max]
  if (!isMultiRange && condition["range"].size() == 2) {
    if (!condition["range"][0].is_number() || !condition["range"][1].is_number()) {
      SMF_LOGE("Invalid range values in condition: " + condition["name"].get<std::string>() +
               ", values are not numbers");
      return false;
    }
    // 确保最小值小于等于最大值
    if (condition["range"][0].get<int>() > condition["range"][1].get<int>()) {
      SMF_LOGE("Min value greater than max value in condition: " +
               condition["name"].get<std::string>());
      return false;
    }
  }
  // 多范围模式：[[min1, max1], [min2, max2], ...]
  else if (isMultiRange) {
    for (size_t i = 0; i < condition["range"].size(); ++i) {
      const auto& range = condition["range"][i];
      if (!range.is_array() || range.size() != 2 || !range[0].is_number() ||
          !range[1].is_number()) {
        SMF_LOGE("Invalid range format in condition: " + condition["name"].get<std::string>() +
                 ", sub-range #" + std::to_string(i));
        return false;
      }
      // 确保每个范围中的最小值小于等于最大值
      if (range[0].get<int>() > range[1].get<int>()) {
        SMF_LOGE("Min value greater than max value in sub-range #" + std::to_string(i) +
                 " in condition: " + condition["name"].get<std::string>());
        return false;
      }
    }
  }
  // 不是有效的范围格式
  else {
    SMF_LOGE("Invalid range format in condition: " + condition["name"].get<std::string>() +
             ", expected [min, max] or [[min1, max1], [min2, max2], ...]");
    return false;
  }

  return true;
}

}  // namespace smf