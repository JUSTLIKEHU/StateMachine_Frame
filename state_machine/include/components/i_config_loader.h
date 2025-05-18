/**
 * @file i_config_loader.h
 * @brief Interface for configuration loading
 * @author xiaokui.hu
 * @date 2025-05-17
 * @details This file defines the interface for the configuration loader component.
 *          The config loader is responsible for loading state machine configuration
 *          from JSON files, including states, events, and transitions.
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

#include <string>

#include "i_component.h"

namespace smf {

class IConfigLoader : public IComponent {
 public:
  virtual ~IConfigLoader() = default;
  virtual bool LoadStateConfig(const std::string& stateConfigFile) = 0;
  virtual bool LoadEventConfig(const std::string& eventConfigDir) = 0;
  virtual bool LoadTransitionConfig(const std::string& transConfigDir) = 0;
  virtual bool LoadConfig(const std::string& configFile) = 0;
};

}  // namespace smf