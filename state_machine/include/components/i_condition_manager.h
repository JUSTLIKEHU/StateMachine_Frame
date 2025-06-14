/**
 * @file i_condition_manager.h
 * @brief Interface for condition management
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file defines the interface for the condition manager component.
 *          The condition manager is responsible for tracking and evaluating conditions
 *          that affect state transitions in the state machine.
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

#include <functional>
#include <string>
#include <vector>

#include "common_define.h"
#include "i_component.h"
namespace smf {

class IConditionManager : public IComponent {
 public:
  virtual ~IConditionManager() = default;
  virtual void SetConditionValue(const std::string& name, int value) = 0;
  virtual void GetConditionValue(const std::string& name, int& value) const = 0;
  virtual bool CheckConditions(const std::vector<ConditionSharedPtr>& conditions,
                               const std::string& op, std::vector<ConditionInfo>& condition_infos) = 0;
  virtual void AddCondition(const ConditionSharedPtr& condition) = 0;
  // 新增：注册条件变化回调
  using ConditionChangeCallback = std::function<void(const std::string&, int, int, bool)>;
  virtual void RegisterConditionChangeCallback(ConditionChangeCallback callback) = 0;
};

}  // namespace smf