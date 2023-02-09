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

/*
 * @file sacn/examples/cpp/source/src/sacn_source_example.h
 * @brief Runs the sACN C++ source example
 *
 * This is a very simple sACN source example. After selecting one or more NICs, a single source is created. The user
 * can add and remove universes. They can also print the network values. This demonstrates use of the sACN Source API
 * and how to handle its callbacks.
 */

#include "sacn_source_example.h"
#include <iostream>
#include <string>
#include <stdio.h>

/**************************************************************************************************
 * Constants
 **************************************************************************************************/

constexpr const uint16_t UNIVERSE_INVALID = 0;
constexpr const uint16_t UNIVERSE_MIN = 1;
constexpr const uint16_t UNIVERSE_MAX = 63999;
constexpr const uint8_t LEVEL_MIN = 0;
constexpr const uint8_t LEVEL_MAX = 255;

/**************************************************************************************************
 * Logging Callback
 **************************************************************************************************/
static void sacn_log_function(void* context, const EtcPalLogStrings* strings)
{
  ETCPAL_UNUSED_ARG(context);
  std::cout << strings->human_readable << "\n";
} // sacn_log_function

/**************************************************************************************************
 * Keyboard
 *Interrupt Handling
 **************************************************************************************************/
bool keep_running = true;
static void handle_keyboard_interrupt()
{
  keep_running = false;
}

extern "C" {
extern void install_keyboard_interrupt_handler(void (*handler)());
}

/* ============================= UniverseInfo ============================== */

void UniverseInfo::SetEffectStateConstant(const uint8_t level)
{
  effect_ = Effect::kConstant;
  for (int i = 0; i < DMX_ADDRESS_COUNT; i++)
  {
    levels_[i] = level;
  }
}  // SetEffectStateConstant

void UniverseInfo::SetEffectStateRamping()
{
  effect_ = Effect::kRamp;
  for (int i = 0; i < DMX_ADDRESS_COUNT; i++)
  {
    levels_[i] = LEVEL_MIN;
  }
}  // SetEffectStateRamping

void UniverseInfo::SetPriorityStateUniverse(const uint8_t universe_priority)
{
  priority_type_ = Priority::kUniverse;
  universe_priority_ = universe_priority;
} // SetPriorityStateUniverse

void UniverseInfo::SetPriorityStatePerAddress(const uint8_t per_address_priority)
{
  priority_type_ = Priority::kPerAddress;
  for (int i = 0; i < DMX_ADDRESS_COUNT; i++)
  {
    per_address_priorities_[i] = per_address_priority;
  }
} // SetPriorityStatePerAddress

void UniverseInfo::IncrementLevels()
{
  uint8_t new_level = 0;
  uint8_t existing_level = levels_[0];
  if (existing_level < LEVEL_MAX)
  {
    new_level = existing_level + 1;
  }
  else
  {
    new_level = 0;
  }
  for (int i = 0; i < DMX_ADDRESS_COUNT; i++)
  {
    levels_[i] = new_level;
  }
} // IncrementLevels

/* =========================== SACNSourceExample =========================== */

SACNSourceExample::SACNSourceExample()
{
  // Handle Ctrl+C gracefully and shut down in compatible consoles
  install_keyboard_interrupt_handler(handle_keyboard_interrupt);
  if (InitEtcPal())
  {
    network_select_.InitializeNics();
    network_select_.SelectNics();
    continue_ramping_ = true;
    if (InitSACNLibrary() && InitSACNSource() && StartRampThread())
    {
      RunSourceExample();
    }
  }
}  // SACNSourceExample

SACNSourceExample::~SACNSourceExample()
{
  continue_ramping_ = false;
  etcpal::Error result = ramp_thread_.Join();
  if (!result)
  {
    std::cout << "Waiting for ramping thread to finish failed, " << result.ToString() << "\n";
  }
  sacn_source_.Shutdown();
  sacn_deinit();
  etcpal_deinit(ETCPAL_FEATURE_NETINTS);
}  // ~SACNSourceExample

bool SACNSourceExample::InitEtcPal()
{
  std::cout << "Initializing ETCPAL... ";
  etcpal::Error result = etcpal_init(ETCPAL_FEATURE_NETINTS);
  if (result)
  {
    std::cout << "success\n";
    return true;
  }
  std::cout << "fail, " << result.ToString() << "\n";
  return false;
} // InitEtcPal

etcpal::Error SACNSourceExample::InitSACNLibrary()
{
  // Initialize the sACN library, allowing it to log messages through our callback
  EtcPalLogParams log_params;
  log_params.action = ETCPAL_LOG_CREATE_HUMAN_READABLE;
  log_params.log_fn = sacn_log_function;
  log_params.time_fn = NULL;
  log_params.log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG);
  std::vector<SacnMcastInterface> netints = network_select_.GetMcastInterfaces();

  std::cout << "Initializing sACN library... ";
  etcpal::Error result = sacn::Init(&log_params, netints);
  if (result)
  {
    std::cout << "success\n";
  }
  else
  {
    std::cout << "failed, " << result.ToString() << "\n";
  }
  return result;
}  // InitSACNLibrary

etcpal::Error SACNSourceExample::InitSACNSource()
{
  etcpal::Uuid my_cid = etcpal::Uuid::V4();
  if (my_cid.IsNull())
  {
    std::cout << "Error: UUID::V4() is not implemented on this platform.\n";
    return kEtcPalErrSys;
  }
  std::string my_name = "sACN C++ Example Source";
  sacn::Source::Settings my_config(my_cid, my_name);
  std::cout << "Starting sACN source... ";
  etcpal::Error result = sacn_source_.Startup(my_config);
  if (result)
  {
    std::cout << "success\n";
  }
  else
  {
    std::cout << "fail, " << result.ToString() << "\n";
  }
  return result.code();
}  // InitSACNSource

void SACNSourceExample::DoRamping()
{
  etcpal::MutexGuard lock(universe_infos_mutex_);
  for (const std::pair<const uint16_t, std::unique_ptr<UniverseInfo>>& pair : universe_infos_)
  {
    uint16_t universe = pair.first;
    UniverseInfo* universe_info = pair.second.get();
    if (universe_info->IsRamping())
    {
      universe_info->IncrementLevels();
      sacn_source_.UpdateLevels(universe, universe_info->levels_, DMX_ADDRESS_COUNT);
    }
  }
}  // DoRamping

void RampFunction(void* arg)
{
  SACNSourceExample* me = (SACNSourceExample*)arg;
  if (!me)
  {
    std::cout << "Error: RampFunction() argument is NULL.\n";
    return;
  }
  while (me->GetContinueRamping())
  {
    me->DoRamping();
    etcpal_thread_sleep(100);  // Sleep for 100 milliseconds
  }
}  // RampFunction

etcpal::Error SACNSourceExample::StartRampThread()
{
  std::cout << "Starting ramp thread... ";
  etcpal::Error result = ramp_thread_.Start(RampFunction, this);
  if (result)
  {
    std::cout << "success\n";
  }
  else
  {
    std::cout << "fail, " << result.ToString() << "\n";
  }
  return result;
}  // StartRampThread

void SACNSourceExample::PrintHelp()
{
  std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";
  std::cout << "Commands\n";
  std::cout << "========\n";
  std::cout << "h : Print help.\n";
  std::cout << "a : Add a new universe.\n";
  std::cout << "r : Remove a universe.\n";
  std::cout << "+ : Add a new unicast address.\n";
  std::cout << "- : Remove a unicast address.\n";
  std::cout << "n : Reset networking.\n";
  std::cout << "q : Exit.\n";
  std::cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n";
}  // PrintHelp

void SACNSourceExample::AddUniverse()
{
  uint16_t new_universe = GetUniverseFromInput();
  if (VerifyNewUniverse(new_universe))
  {
    std::unique_ptr<UniverseInfo> new_universe_info = std::make_unique<UniverseInfo>();
    bool ctrl_c_pressed;
    int effect = GetSingleCharFromInput("Enter effect:\nc : constant\nr : ramp\n", {'c', 'r'}, &ctrl_c_pressed);
    if (ctrl_c_pressed)
    {
      return;
    }
    if (effect == 'c')
    {
      uint8_t level = GetUint8FromInput(LEVEL_MIN, LEVEL_MAX, "Level");
      new_universe_info->SetEffectStateConstant(level);
    }
    else if (effect == 'r')
    {
      new_universe_info->SetEffectStateRamping();
    }
    int priority_type = GetSingleCharFromInput("Enter priority:\nu : universe\na : per address\n", {'u', 'a'}, &ctrl_c_pressed);
    if (ctrl_c_pressed)
    {
      return;
    }
    if (priority_type == 'u')
    {
      new_universe_info->SetPriorityStateUniverse(GetUniversePriorityFromInput());
    }
    else if (priority_type == 'a')
    {
      new_universe_info->SetPriorityStatePerAddress(GetPerAddressPriorityFromInput());
    }
    if (AddNewUniverseToSACNSource(new_universe, new_universe_info))
    {
      etcpal::MutexGuard lock(universe_infos_mutex_);
      universe_infos_[new_universe] = std::move(new_universe_info);
    }
  }
} // AddUniverse

bool SACNSourceExample::VerifyNewUniverse(const uint16_t new_universe)
{
  /* Note: sacn_source_.AddUniverse() will check for duplicate universe and return
   * an error. This is a convenience for the user. */
  std::vector<uint16_t> existing_universes = sacn_source_.GetUniverses();
  if (std::find(existing_universes.begin(), existing_universes.end(), new_universe) == existing_universes.end())
  {
    return true;
  }
  std::cout << "Universe " << new_universe << " already exists.\n";
  return false;
}  // VerifyNewUniverse

bool SACNSourceExample::AddNewUniverseToSACNSource(const uint16_t new_universe,
                                                   const std::unique_ptr<UniverseInfo>& new_universe_info)
{
  std::vector<SacnMcastInterface> netints = network_select_.GetMcastInterfaces();
  std::cout << "Adding universe " << new_universe << "... ";
  etcpal::Error result = sacn_source_.AddUniverse(sacn::Source::UniverseSettings(new_universe), netints);
  if (result)
  {
    bool success = true;
    for (SacnMcastInterface netint : netints)
    {
      if (netint.status != kEtcPalErrOk)
      {
        success = false;
        std::cout << "fail, " << etcpal_strerror(netint.status) << "\n";
      }
    }
    if (success)
    {
      std::cout << "success\n";
      if (new_universe_info->IsUniversePriority())
      {
        std::cout << "Setting universe priority... ";
        etcpal::Error local_result = sacn_source_.ChangePriority(new_universe, new_universe_info->universe_priority_);
        if (local_result)
        {
          std::cout << "success\n";
          std::cout << "Setting levels... ";
          sacn_source_.UpdateLevels(new_universe, new_universe_info->levels_, DMX_ADDRESS_COUNT);
          std::cout << "success\n";
        }
        else
        {
          std::cout << "fail, " << local_result.ToString() << "\n";
        }
      }
      else
      {
        std::cout << "Setting levels and per address priorities... ";
        sacn_source_.UpdateLevelsAndPap(new_universe, new_universe_info->levels_, DMX_ADDRESS_COUNT,
                                        new_universe_info->per_address_priorities_, DMX_ADDRESS_COUNT);
        std::cout << "success\n";
      }
      return true;
    }
  }
  else
  {
    std::cout << "fail, " << result.ToString() << "\n";
  }
  return false;
} // AddNewUniverseToSACNSource

void SACNSourceExample::RemoveUniverse()
{
  uint16_t universe = GetUniverseFromInput();
  RemoveUniverseCommon(universe);
}  // RemoveUniverse

void SACNSourceExample::RemoveUniverseCommon(uint16_t universe)
{
  std::vector<uint16_t> existing_universes = sacn_source_.GetUniverses();
  if (std::find(existing_universes.begin(), existing_universes.end(), universe) != existing_universes.end())
  {
    std::cout << "Removing universe " << universe << "... ";
    sacn_source_.RemoveUniverse(universe);
    std::cout << "success\n";
    etcpal::MutexGuard lock(universe_infos_mutex_);
    universe_infos_.erase(universe);
  }
  else
  {
    std::cout << "Universe " << universe << " not found.\n";
  }
} // RemoveUniverseCommon

void SACNSourceExample::AddUnicastAddress()
{
  uint16_t universe = GetUniverseFromInput();
  etcpal::IpAddr address = GetIPAddressFromInput();
  std::cout << "Adding address... ";
  etcpal::Error result = sacn_source_.AddUnicastDestination(universe, address);
  if (result == kEtcPalErrOk)
  {
    std::cout << "success\n";
  }
  else
  {
    std::cout << "fail, " << result.ToString() << "\n";
  }
} // AddUnicastAddress

void SACNSourceExample::RemoveUnicastAddress()
{
  uint16_t universe = GetUniverseFromInput();
  std::vector<uint16_t> existing_universes = sacn_source_.GetUniverses();
  if (std::find(existing_universes.begin(), existing_universes.end(), universe) != existing_universes.end())
  {
    etcpal::IpAddr address = GetIPAddressFromInput();
    std::vector<etcpal::IpAddr> addresses = sacn_source_.GetUnicastDestinations(universe);
    if (std::find(addresses.begin(), addresses.end(), address) != addresses.end())
    {
      std::cout << "Removing address... ";
      sacn_source_.RemoveUnicastDestination(universe, address);
      std::cout << "success\n";
    }
    else
    {
      std::cout << "Address " << address.ToString() << " not found.\n";
    }
  }
  else
  {
    std::cout << "Universe " << universe << " not found.\n";
  }
} // RemoveUnicastAddress

void SACNSourceExample::ResetNetworking()
{
  std::vector<SacnMcastInterface> interfaces = network_select_.GetMcastInterfaces();
  std::cout << "Resetting network interface(s)... ";
  etcpal::Error result = sacn_source_.ResetNetworking(interfaces);
  if (result == kEtcPalErrOk)
  {
    std::cout << "success\n";
  }
  else
  {
    std::cout << "fail, " << result.ToString() << "\n";
  }
} // ResetNetworking

void SACNSourceExample::RunSourceExample()
{
  // Handle user input until we are told to stop
  while (keep_running)
  {
    bool ctrl_c_pressed;
    int ch = GetSingleCharFromInput("Enter input (enter h for help):\n", {'h', 'a', 'r', '+', '-', 'n', 'q'},
                                    &ctrl_c_pressed);
    if (ctrl_c_pressed)
    {
      keep_running = false;
    }
    switch (ch)
    {
      case 'h':
        PrintHelp();
        break;
      case 'a':
        AddUniverse();
        break;
      case 'r':
        RemoveUniverse();
        break;
      case '+':
        AddUnicastAddress();
        break;
      case '-':
        RemoveUnicastAddress();
        break;
      case 'n':
        ResetNetworking();
        break;
      case 'q':
        keep_running = false;
        break;
    }
  }
}  // RunSourceExample

/* utility functions */

uint8_t SACNSourceExample::GetUint8FromInput(const uint8_t min, const uint8_t max, const std::string label)
{
  uint8_t value = min;
  bool print_prompt = true;
  bool finished = false;
  while (!finished)
  {
    std::string s;
    if (print_prompt)
    {
      std::cout << label << " (" << (unsigned int)min << " - " << (unsigned int)max << "): ";
    }
    std::getline(std::cin, s);
    if (s == "")
    {
      print_prompt = false;
      continue;
    }
    print_prompt = true;
    int temp = atoi(s.c_str());
    if ((temp >= min) && (temp <= max))
    {
      value = (uint8_t)temp;
      finished = true;
    }
  }
  return value;
} // GetUint8FromInput

uint16_t SACNSourceExample::GetUniverseFromInput()
{
  bool print_prompt = true;
  uint16_t universe = UNIVERSE_INVALID;
  while (universe == UNIVERSE_INVALID)
  {
    std::string universe_string;
    if (print_prompt)
    {
      std::cout << "Universe (" << UNIVERSE_MIN << " - " << UNIVERSE_MAX << "): ";
    }
    std::getline(std::cin, universe_string);
    if (universe_string == "")
    {
      print_prompt = false;
      continue;
    }
    print_prompt = true;
    int temp = atoi(universe_string.c_str());
    if ((temp >= UNIVERSE_MIN) && (temp <= UNIVERSE_MAX))
    {
      universe = (uint16_t)temp;
    }
  }
  return universe;
}  // GetUniverseFromInput

#define UNIVERSE_PRIORITY_MIN 0
#define UNIVERSE_PRIORITY_MAX 200
uint8_t SACNSourceExample::GetUniversePriorityFromInput()
{
  return GetUint8FromInput(UNIVERSE_PRIORITY_MIN, UNIVERSE_PRIORITY_MAX, "Universe Priority");
}  // GetUniversePriorityFromInput

#define PER_ADDRESS_PRIORITY_MIN 0
#define PER_ADDRESS_PRIORITY_MAX 200
uint8_t SACNSourceExample::GetPerAddressPriorityFromInput()
{
  return GetUint8FromInput(PER_ADDRESS_PRIORITY_MIN, PER_ADDRESS_PRIORITY_MAX, "Per Address Priority");
}  // GetPerAddressPriorityFromInput

int SACNSourceExample::GetSingleCharFromInput(const std::string prompt, const std::vector<int> valid_letters,
                                              bool* ctrl_c_pressed)
{
  *ctrl_c_pressed = false;
  bool print_prompt = true;
  while (true)
  {
    if (print_prompt)
    {
      std::cout << prompt;
    }
    else
    {
      print_prompt = true;  // Print it next time by default.
    }
    int ch = getchar();
    if (ch == EOF)
    {
      *ctrl_c_pressed = true;
      return 0;
    }
    if (std::find(valid_letters.begin(), valid_letters.end(), ch) != valid_letters.end())
    {
      return ch;
    }
    if (ch == '\n')
    {
      print_prompt = false;  // Otherwise the prompt is printed twice.
    }
    else
    {
      std::cout << "Invalid input.\n";
    }
  }
  return 0;
}  // GetSingleCharFromInput

etcpal::IpAddr SACNSourceExample::GetIPAddressFromInput()
{
  while (true)
  {
    std::cout << "IP address: ";
    std::string address_string;
    std::getline(std::cin, address_string);
    etcpal::IpAddr address = etcpal::IpAddr::FromString(address_string);
    if (address.IsValid())
    {
      return address;
    }
    std::cout << "Address " << address_string << " is not valid.\n";
  }
}  // GetIPAddressFromInput
