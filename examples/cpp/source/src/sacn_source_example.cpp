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

#define LEVEL_MIN 0
#define LEVEL_MAX 255
#define BEGIN_BORDER_STRING \
  ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"
#define END_BORDER_STRING \
  "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n"

/**************************************************************************************************
 * Logging Callback
 **************************************************************************************************/
static void log_callback(void* context, const EtcPalLogStrings* strings)
{
  ETCPAL_UNUSED_ARG(context);
  std::cout << strings->human_readable << "\n";
}

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

#define UNIVERSE_INVALID 0
#define UNIVERSE_MIN 1
#define UNIVERSE_MAX 63999
SACNSourceExample::SACNSourceExample()
{
  new_universe_ = UNIVERSE_INVALID;
  if (etcpal_rwlock_create(&universe_infos_lock_))
  {
    // Handle Ctrl+C gracefully and shut down in compatible consoles
    install_keyboard_interrupt_handler(handle_keyboard_interrupt);
    if (etcpal_init(ETCPAL_FEATURE_NETINTS) == kEtcPalErrOk)
    {
      network_select_.getNICs();
      network_select_.selectNICs();
      if (initSACNLibrary() == kEtcPalErrOk)
      {
        if (initSACNSource() == kEtcPalErrOk)
        {
          continue_ramping_ = true;
          if (startRampThread() == kEtcPalErrOk)
          {
            runSourceExample();
          }
        }
      }
    }
    etcpal_rwlock_destroy(&universe_infos_lock_);
  }
  else
  {
    std::cout << "Error creating read / write lock\n";
  }
}  // SACNSourceExample

SACNSourceExample::~SACNSourceExample()
{
  continue_ramping_ = false;
  etcpal_error_t result = etcpal_thread_join(&ramp_thread_handle_);  // wait for thread to finish
  if (result != kEtcPalErrOk)
  {
    std::cout << "Waiting for ramping thread to finish failed, " << etcpal_strerror(result) << "\n";
  }
  sacn_source_.Shutdown();
  sacn_deinit();
}  // ~SACNSourceExample

etcpal_error_t SACNSourceExample::initSACNLibrary()
{
  // Initialize the sACN library, allowing it to log messages through our callback
  EtcPalLogParams log_params;
  log_params.action = ETCPAL_LOG_CREATE_HUMAN_READABLE;
  log_params.log_fn = log_callback;
  log_params.time_fn = NULL;
  log_params.log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG);
  SacnNetintConfig sys_netint_config;
  std::vector<SacnMcastInterface> netints;
  network_select_.getMcastInterfaces(netints);
  sys_netint_config.netints = &(netints[0]);
  sys_netint_config.num_netints = netints.size();
  sys_netint_config.no_netints = false;

  std::cout << "Initializing sACN library... ";
  etcpal_error_t result = sacn_init(&log_params, &sys_netint_config);
  if (result == kEtcPalErrOk)
  {
    std::cout << "success\n";
  }
  else
  {
    std::cout << "failed, " << etcpal_strerror(result) << "\n";
  }
  return result;
}  // initSACNLibrary

etcpal_error_t SACNSourceExample::initSACNSource()
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
  if (result == kEtcPalErrOk)
  {
    std::cout << "success\n";
  }
  else
  {
    std::cout << "fail, " << result.ToString() << "\n";
  }
  return result.code();
}  // initSACNSource

void SACNSourceExample::doRamping()
{
  if (etcpal_rwlock_writelock(&universe_infos_lock_))
  {
    for (const std::pair<const uint16_t, std::unique_ptr<UniverseInfo>>& pair : universe_infos_)
    {
      uint16_t universe = pair.first;
      UniverseInfo* universe_info = pair.second.get();
      if (universe_info->effect == kEffectRamp)
      {
        uint8_t new_level = 0;
        uint8_t existing_level = universe_info->levels[DMX_ADDRESS_COUNT - 1];
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
          // 'universe_info' is a copy
          // universe_info.levels[i] = new_level;
          universe_infos_[universe]->levels[i] = new_level;
        }
        if (universe_info->priority_type == kUniversePriority)
        {
          sacn_source_.UpdateLevels(universe, universe_info->levels, DMX_ADDRESS_COUNT);
        }
        else
        {
          sacn_source_.UpdateLevelsAndPap(universe, universe_info->levels, DMX_ADDRESS_COUNT,
                                          universe_info->per_address_priorities,
                                          DMX_ADDRESS_COUNT);
        }
      }
    }
    etcpal_rwlock_writeunlock(&universe_infos_lock_);
  }
  else
  {
    std::cout << "doRamping: error getting write lock\n";
  }
}  // doRamping

void ramp_function(void* arg)
{
  SACNSourceExample* me = (SACNSourceExample*)arg;
  if (!me)
  {
    std::cout << "Error: ramp_function() argument is NULL.\n";
    return;
  }
  while (me->getContinueRamping())
  {
    me->doRamping();
    etcpal_thread_sleep(100);  // Sleep for 100 milliseconds
  }
}  // ramp_function

etcpal_error_t SACNSourceExample::startRampThread()
{
  EtcPalThreadParams params = ETCPAL_THREAD_PARAMS_INIT;
  std::cout << "Starting ramp thread... ";
  etcpal_error_t result = etcpal_thread_create(&ramp_thread_handle_, &params, ramp_function, this);
  if (result == kEtcPalErrOk)
  {
    std::cout << "success\n";
  }
  else
  {
    std::cout << "fail, " << etcpal_strerror(result) << "\n";
  }
  return result;
}  // startRampThread

void SACNSourceExample::printHelp()
{
  std::cout << BEGIN_BORDER_STRING;
  std::cout << "Commands\n";
  std::cout << "========\n";
  std::cout << "h : Print help.\n";
  std::cout << "a : Add a new universe.\n";
  std::cout << "r : Remove a universe.\n";
  std::cout << "+ : Add a new unicast address.\n";
  std::cout << "- : Remove a unicast address.\n";
  std::cout << "n : Reset networking.\n";
  std::cout << "q : Exit.\n";
  std::cout << END_BORDER_STRING;
}  // printHelp

uint16_t SACNSourceExample::getUniverse()
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
}  // getUniverse

bool SACNSourceExample::addUniverse()
{
  new_universe_ = UNIVERSE_INVALID;
  new_universe_info_ = std::make_unique<UniverseInfo>();
  uint16_t universe = getUniverse();
  std::vector<uint16_t> existing_universes = sacn_source_.GetUniverses();
  if (std::find(existing_universes.begin(), existing_universes.end(), universe) == existing_universes.end())
  {
    new_universe_ = universe;
    return true;
  }
  std::cout << "Universe " << universe << " already exists.\n";
  return false;
}  // addUniverse

uint8_t SACNSourceExample::getLevel()
{
  return getUint8(LEVEL_MIN, LEVEL_MAX, "Level");
}  // getLevel

void SACNSourceExample::setEffect()
{
  int ch = getSingleChar("Enter effect:\nc : constant\nr : ramp\n", {'c', 'r'});
  if (ch == 'c')
  {
    new_universe_info_->effect = kEffectConstant;
    uint8_t level = getLevel();
    for (int i = 0; i < DMX_ADDRESS_COUNT; i++)
    {
      new_universe_info_->levels[i] = level;
    }
  }
  else if (ch == 'r')
  {
    new_universe_info_->effect = kEffectRamp;
    for (int i = 0; i < DMX_ADDRESS_COUNT; i++)
    {
      new_universe_info_->levels[i] = LEVEL_MIN;
    }
  }
}  // setEffect

#define UNIVERSE_PRIORITY_MIN 0
#define UNIVERSE_PRIORITY_MAX 200
uint8_t SACNSourceExample::getUniversePriority()
{
  return getUint8(UNIVERSE_PRIORITY_MIN, UNIVERSE_PRIORITY_MAX, "Universe Priority");
}  // getUniversePriority

#define PER_ADDRESS_PRIORITY_MIN 0
#define PER_ADDRESS_PRIORITY_MAX 200
uint8_t SACNSourceExample::getPerAddressPriority()
{
  return getUint8(PER_ADDRESS_PRIORITY_MIN, PER_ADDRESS_PRIORITY_MAX, "Per Address Priority");
}  // getPerAddressPriority

bool SACNSourceExample::setPriority()
{
  bool success = true;
  int ch = getSingleChar("Enter priority:\nu : universe\na : per address\n", {'u', 'a'});
  if (ch == 'u')
  {
    new_universe_info_->priority_type = kUniversePriority;
    new_universe_info_->universe_priority = getUniversePriority();
  }
  else if (ch == 'a')
  {
    new_universe_info_->priority_type = kPerAddressPriority;
    uint8_t priority = getPerAddressPriority();
    for (int i = 0; i < DMX_ADDRESS_COUNT; i++)
    {
      new_universe_info_->per_address_priorities[i] = priority;
    }
  }
  return success;
}  // setPriority

bool SACNSourceExample::addNewUniverseToSACNSource()
{
  std::vector<SacnMcastInterface> netints;
  network_select_.getMcastInterfaces(netints);
  std::cout << "Adding universe " << new_universe_ << "... ";
  etcpal::Error result = sacn_source_.AddUniverse(sacn::Source::UniverseSettings(new_universe_), netints);
  if (result == kEtcPalErrOk)
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
      if (new_universe_info_->priority_type == kUniversePriority)
      {
        sacn_source_.ChangePriority(new_universe_, new_universe_info_->universe_priority);
        sacn_source_.UpdateLevels(new_universe_, new_universe_info_->levels, DMX_ADDRESS_COUNT);
      }
      else
      {
        sacn_source_.UpdateLevelsAndPap(new_universe_, new_universe_info_->levels, DMX_ADDRESS_COUNT,
                                        new_universe_info_->per_address_priorities, DMX_ADDRESS_COUNT);
      }
      return true;
    }
  }
  else
  {
    std::cout << "fail, " << result.ToString() << "\n";
  }
  return false;
} // addNewUniverseToSACNSource

void SACNSourceExample::removeUniverse()
{
  uint16_t universe = getUniverse();
  removeUniverseCommon(universe);
}  // removeUniverse

void SACNSourceExample::removeUniverseCommon(uint16_t universe)
{
  std::vector<uint16_t> existing_universes = sacn_source_.GetUniverses();
  if (std::find(existing_universes.begin(), existing_universes.end(), universe) != existing_universes.end())
  {
    if (etcpal_rwlock_writelock(&universe_infos_lock_))
    {
      std::cout << "Removing universe " << universe << "... ";
      sacn_source_.RemoveUniverse(universe);
      std::cout << "success\n";
      universe_infos_.erase(universe);
      etcpal_rwlock_writeunlock(&universe_infos_lock_);
    }
    else
    {
      std::cout << "removeUniverse: error getting write lock\n";
    }
  }
  else
  {
    std::cout << "Universe " << universe << " not found.\n";
  }
}

void SACNSourceExample::addUnicastAddress()
{
  uint16_t universe = getUniverse();
  std::vector<uint16_t> existing_universes = sacn_source_.GetUniverses();
  if (std::find(existing_universes.begin(), existing_universes.end(), universe) != existing_universes.end())
  {
    etcpal::IpAddr address = getIPAddress();
    std::vector<etcpal::IpAddr> addresses = sacn_source_.GetUnicastDestinations(universe);
    if (std::find(addresses.begin(), addresses.end(), address) == addresses.end())
    {
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
    }
    else
    {
      std::cout << "Address " << address.ToString().c_str() << " already found.\n";
    }
  }
  else
  {
    std::cout << "Universe " << universe << " not found.\n";
  }
} // addUnicastAddress

void SACNSourceExample::removeUnicastAddress()
{
  uint16_t universe = getUniverse();
  std::vector<uint16_t> existing_universes = sacn_source_.GetUniverses();
  if (std::find(existing_universes.begin(), existing_universes.end(), universe) != existing_universes.end())
  {
    etcpal::IpAddr address = getIPAddress();
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
} // removeUnicastAddress

void SACNSourceExample::resetNetworking()
{
  std::vector<SacnMcastInterface> interfaces;
  network_select_.getMcastInterfaces(interfaces);
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
}

void SACNSourceExample::runSourceExample()
{
  // Handle user input until we are told to stop
  while (keep_running)
  {
    int ch = getSingleChar("Enter input (enter h for help):\n", {'h', 'a', 'r', '+', '-', 'n', 'q'});
    switch (ch)
    {
      case 'h':
        printHelp();
        break;
      case 'a':
        if (addUniverse())
        {
          setEffect();
          if (setPriority())
          {
            if (addNewUniverseToSACNSource())
            {
              if (etcpal_rwlock_writelock(&universe_infos_lock_))
              {
                universe_infos_[new_universe_] = std::move(new_universe_info_);
                etcpal_rwlock_writeunlock(&universe_infos_lock_);
              }
              else
              {
                std::cout << "Error getting write lock\n";
              }
            }
          }
        }
        break;
      case 'r':
        removeUniverse();
        break;
      case '+':
        addUnicastAddress();
        break;
      case '-':
        removeUnicastAddress();
        break;
      case 'n':
        resetNetworking();
        break;
      case 'q':
        keep_running = false;
        break;
    }
  }
}  // runSourceExample

uint8_t SACNSourceExample::getUint8(const uint8_t min, const uint8_t max, const std::string label)
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
} // getUint8

int SACNSourceExample::getSingleChar(const std::string prompt, const std::vector<int> valid_letters)
{
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
}  // getSingleChar

std::vector<std::string> SACNSourceExample::split(const std::string& s, char separator)
{
  std::vector<std::string> tokens;
  std::string::size_type prev_pos = 0;
  std::string::size_type pos = 0;
  while ((pos = s.find(separator, pos)) != std::string::npos)
  {
    std::string substring(s.substr(prev_pos, pos - prev_pos));
    tokens.push_back(substring);
    prev_pos = ++pos;
  }
  tokens.push_back(s.substr(prev_pos, pos - prev_pos));
  return tokens;
} // split

etcpal::IpAddr SACNSourceExample::getIPAddress()
{
  etcpal::IpAddr address;
  bool success = false;
  while (!success)
  {
    std::cout << "IP address: ";
    std::string address_string;
    std::getline(std::cin, address_string);
    std::vector<std::string> tokens = split(address_string, '.');
    if (tokens.size() == 4)
    {
      bool local_success = true;
      uint32_t v4_data = 0;
      for (int i = 0; i < 4; i++)
      {
        int token = atoi(tokens.at(i).c_str());
        if ((token >= 0) && (token <= 255))
        {
          v4_data = (v4_data * 256) + token;
        }
        else
        {
          local_success = false;
          break;
        }
      }
      if (local_success)
      {
        address = etcpal::IpAddr(v4_data);
        if (address.IsValid())
        {
          success = true;
        }
      }
    }
  }
  return address;
}  // getIPAddress
