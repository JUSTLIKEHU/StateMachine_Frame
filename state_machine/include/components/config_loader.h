/**
 * @file config_loader.h
 * @brief Configuration loader implementation
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file contains the implementation of the configuration loader component,
 *          which is responsible for loading state machine configuration from JSON files,
 *          including states, events, and transitions.
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

#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common_define.h"
#include "i_config_loader.h"
#include "nlohmann-json/json.hpp"
namespace smf {

using json = nlohmann::json;

// 前向声明
class IStateManager;
class IConditionManager;
class IEventHandler;
class ITransitionManager;

class ConfigLoader : public IConfigLoader {
 public:
  ConfigLoader(IStateManager* state_manager, IConditionManager* condition_manager,
               ITransitionManager* transition_manager, IEventHandler* event_handler);
  ~ConfigLoader() override;

  // IComponent interface
  void Start() override;
  void Stop() override;
  bool IsRunning() const override;

  // IConfigLoader interface
  bool LoadStateConfig(const std::string& stateConfigFile) override;
  bool LoadEventConfig(const std::string& eventConfigDir) override;
  bool LoadTransitionConfig(const std::string& transConfigDir) override;
  bool LoadConfig(const std::string& configFile) override;

 private:
  // 配置验证方法
  bool ValidateStateConfig(const json& config) const;
  bool ValidateEventConfig(const json& config) const;
  bool ValidateTransitionConfig(const json& config) const;

  bool ValidateCondition(const json& condition) const;

  // 配置解析方法
  bool ParseStateConfig(const json& config);
  bool ParseEventConfig(const json& config);
  bool ParseTransitionConfig(const json& config);

  // 辅助方法
  bool IsValidState(const std::string& state) const;
  bool IsValidEvent(const std::string& event) const;
  bool IsValidCondition(const Condition& condition) const;

  // 文件操作方法
  bool LoadJsonFile(const std::string& filePath, json& jsonData);
  std::vector<std::string> GetJsonFilesInDirectory(const std::string& dirPath);

 private:
  // 组件引用
  IStateManager* state_manager_;
  IConditionManager* condition_manager_;
  ITransitionManager* transition_manager_;
  IEventHandler* event_handler_;
  // 状态名称
  std::set<std::string> state_names_;

  // 运行状态
  std::atomic_bool running_{false};
};

}  // namespace smf