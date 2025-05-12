/**
 * @file state_machine_factory.cpp
 * @brief 状态机工厂类实现
 * @author xiaokui.hu
 * @date 2025-05-11
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

#include "state_machine_factory.h"
#include <memory>
#include "logger.h"
namespace smf {

std::unordered_map<std::string, std::shared_ptr<FiniteStateMachine>>
    StateMachineFactory::state_machines_;

std::vector<std::string> StateMachineFactory::GetAllStateMachineNames() {
  std::vector<std::string> names;
  for (const auto& [name, _] : state_machines_) {
    names.push_back(name);
  }
  return names;
}

std::shared_ptr<FiniteStateMachine> StateMachineFactory::CreateStateMachine(
    const std::string& name) {
  if (state_machines_.find(name) != state_machines_.end()) {
    SMF_LOGW("State machine with name:  " + name + " already exists");
    return state_machines_[name];
  }
  auto state_machine = std::shared_ptr<FiniteStateMachine>(new FiniteStateMachine(name));
  state_machines_[name] = state_machine;
  return state_machine;
}

std::shared_ptr<FiniteStateMachine> StateMachineFactory::GetStateMachine(const std::string& name) {
  if (state_machines_.find(name) == state_machines_.end()) {
    SMF_LOGE("State machine with name:  " + name + " not found");
    return nullptr;
  }
  return state_machines_[name];
}

std::unordered_map<std::string, std::shared_ptr<FiniteStateMachine>>
StateMachineFactory::GetAllStateMachines() {
  return state_machines_;
}

}  // namespace smf
