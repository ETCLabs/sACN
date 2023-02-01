/******************************************************************************
 * Copyright 2022 ETC Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of sACN. For more information, go to:
 * https://github.com/ETCLabs/sACN
 *****************************************************************************/

#ifndef SACN_SOURCE_EXAMPLE_H_
#define SACN_SOURCE_EXAMPLE_H_

/**
 * @file sacn/examples/cpp/source/src/sacn_source_example.h
 * @brief Runs the sACN C++ source example
 */

#include <unordered_map>
#include "network_select.h"
#include "sacn/cpp/source.h"
#include "etcpal/thread.h"
#include "etcpal/rwlock.h"

class SACNSourceExample
{
public:
  typedef enum
  {
    kEffectConstant,
    kEffectRamp
  } effect_t;

  typedef enum
  {
    kUniversePriority,
    kPerAddressPriority
  } priority_type_t;

  typedef struct UniverseInfo
  {
    effect_t effect;
    priority_type_t priority_type;
    uint8_t universe_priority;
    uint8_t per_address_priorities[DMX_ADDRESS_COUNT];
    uint8_t levels[DMX_ADDRESS_COUNT];

    UniverseInfo() = default;
  } UniverseInfo;

  SACNSourceExample();
  ~SACNSourceExample();
  void doRamping();
  bool getContinueRamping() { return continue_ramping_; }

private:
  etcpal_error_t initSACNLibrary();
  etcpal_error_t initSACNSource();
  etcpal_error_t startRampThread();
  void printHelp();
  uint16_t getUniverse();
  bool addUniverse();
  uint8_t getLevel();
  void setEffect();
  uint8_t getUniversePriority();
  uint8_t getPerAddressPriority();
  bool setPriority();
  bool addNewUniverseToSACNSource();
  void removeUniverse();
  void removeUniverseCommon(uint16_t universe);
  void addUnicastAddress();
  void removeUnicastAddress();
  void resetNetworking();
  void runSourceExample();

  /* utility functions */
  uint8_t getUint8(const uint8_t min, const uint8_t max, const std::string label);
  int getSingleChar(const std::string prompt, const std::vector<int> valid_letters);
  std::vector<std::string> split(const std::string& s, char separator);
  etcpal::IpAddr getIPAddress();

  NetworkSelect network_select_;
  sacn::Source sacn_source_;
  uint16_t new_universe_;
  std::unique_ptr<UniverseInfo> new_universe_info_;
  etcpal_rwlock_t universe_infos_lock_;
  std::unordered_map < uint16_t, std::unique_ptr<UniverseInfo>> universe_infos_;
  bool continue_ramping_;
  etcpal_thread_t ramp_thread_handle_;
};

#endif  // SACN_SOURCE_EXAMPLE_H_
