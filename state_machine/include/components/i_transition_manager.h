/**
 * @file i_transition_manager.h
 * @brief Interface for transition management
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file defines the interface for the transition manager component.
 *          The transition manager is responsible for storing and retrieving state
 *          transition rules that define when and how states can change.
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

#include <memory>

#include "common_define.h"
#include "event.h"
#include "i_component.h"

namespace smf {

class ITransitionManager : public IComponent {
 public:
  virtual ~ITransitionManager() = default;
  virtual bool AddTransition(const TransitionRule& rule) = 0;
  virtual bool FindTransition(const State& current_state, const EventPtr& event,
                              std::vector<TransitionRule>& out_rules) = 0;
  virtual void Clear() = 0;
};

}  // namespace smf