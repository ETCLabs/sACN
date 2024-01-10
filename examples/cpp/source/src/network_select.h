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

#ifndef NETWORK_SELECT_H_
#define NETWORK_SELECT_H_

/**
 * @file sacn/examples/cpp/source/src/network_select.h
 * @brief Holds network interface selection information
 */

#include <list>
#include <memory>
#include <vector>
#include "etcpal/cpp/inet.h"
#include "etcpal/netint.h"
#include "sacn/cpp/common.h"


class NetworkSelect
{
public:
  NetworkSelect();
  void InitializeNics(void);
  void SelectNics(void);
  std::vector<SacnMcastInterface> GetMcastInterfaces(void) const;

private:
  struct EtcPalNetintInfoSelect
  {
    bool selected;
    char ui_index;
    unsigned int os_index;
    etcpal::IpAddr addr;
    std::string name;
    std::string addr_string;
  };

  void PrintNics(void) const;
  bool IsAnyNicSelected(void) const;

  std::list<std::unique_ptr<EtcPalNetintInfoSelect>> all_network_interfaces_;
};

#endif  // NETWORK_SELECT_H_
