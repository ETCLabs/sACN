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

class UniverseInfo
{
public:
  UniverseInfo() = default;
  void SetEffectStateConstant(const uint8_t level);
  void SetEffectStateRamping();
  void SetPriorityStateUniverse(const uint8_t universe_priority);
  void SetPriorityStatePerAddress(const uint8_t per_address_priority);
  void IncrementLevels();

  effect_t effect_;
  priority_type_t priority_type_;
  uint8_t universe_priority_;
  uint8_t per_address_priorities_[DMX_ADDRESS_COUNT];
  uint8_t levels_[DMX_ADDRESS_COUNT];
};

class SACNSourceExample
{
public:

  SACNSourceExample();
  ~SACNSourceExample();
  void DoRamping();
  bool GetContinueRamping() { return continue_ramping_; }

private:
  etcpal::Error InitSACNLibrary();
  etcpal_error_t InitSACNSource();
  etcpal_error_t StartRampThread();
  void PrintHelp();
  void AddUniverse();
  bool VerifyNewUniverse(uint16_t new_universe);
  bool AddNewUniverseToSACNSource(const uint16_t new_universe, const std::unique_ptr<UniverseInfo>& new_universe_info);
  void RemoveUniverse();
  void RemoveUniverseCommon(uint16_t universe);
  void AddUnicastAddress();
  void RemoveUnicastAddress();
  void ResetNetworking();
  void RunSourceExample();

  /* utility functions */
  uint8_t GetUint8FromInput(const uint8_t min, const uint8_t max, const std::string label);
  uint16_t GetUniverseFromInput();
  uint8_t GetUniversePriorityFromInput();
  uint8_t GetPerAddressPriorityFromInput();
  int GetSingleCharFromInput(const std::string prompt, const std::vector<int> valid_letters);
  std::vector<std::string> split(const std::string& s, char separator);
  etcpal::IpAddr GetIPAddressFromInput();

  NetworkSelect network_select_;
  sacn::Source sacn_source_;
  etcpal_rwlock_t universe_infos_lock_;
  std::unordered_map < uint16_t, std::unique_ptr<UniverseInfo>> universe_infos_;
  bool continue_ramping_;
  etcpal_thread_t ramp_thread_handle_;
};

#endif  // SACN_SOURCE_EXAMPLE_H_
