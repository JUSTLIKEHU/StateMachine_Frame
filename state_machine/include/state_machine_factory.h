/**
 * @file state_machine_factory.h
 * @brief 状态机工厂类
 * @author xiaokui.hu
 * @date 2025-05-11
 * @details 状态机工厂类，用于创建状态机
 */

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

#include "state_machine.h"

namespace smf {

class StateMachineFactory {
 public:
  static std::shared_ptr<FiniteStateMachine> CreateStateMachine(const std::string& name);
  
  static std::vector<std::string> GetAllStateMachineNames();

  static std::shared_ptr<FiniteStateMachine> GetStateMachine(const std::string& name);

  static std::unordered_map<std::string, std::shared_ptr<FiniteStateMachine>> GetAllStateMachines();
  

 private:
  static std::unordered_map<std::string, std::shared_ptr<FiniteStateMachine>> state_machines_;
};

}  // namespace smf
