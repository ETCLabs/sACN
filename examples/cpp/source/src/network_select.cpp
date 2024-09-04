/******************************************************************************
 * Copyright 2024 ETC Inc.
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

#include "network_select.h"

#include "etcpal/cpp/netint.h"
#include <iostream>

void NetworkSelect::InitializeNics()
{
  auto netints = etcpal::netint::GetInterfaces();
  if (netints)
  {
    char char_index = 'a';
    for (const auto& netint : *netints)
    {
      if (!netint.friendly_name().empty() && netint.addr().IsValid())
      {
        std::unique_ptr<EtcPalNetintInfoSelect> network_interface = std::make_unique<EtcPalNetintInfoSelect>();
        network_interface->selected                               = false;
        network_interface->ui_index                               = char_index++;
        network_interface->os_index                               = netint.index();
        network_interface->addr                                   = netint.addr();
        network_interface->addr_string                            = netint.addr().ToString();
        network_interface->name                                   = netint.friendly_name();
        all_network_interfaces_.push_back(std::move(network_interface));
      }
    }
  }
}  // InitializeNics

void NetworkSelect::PrintNics() const
{
  std::cout << "Selected Index Network Interface\n";
  std::cout << "======== ===== =================\n";
  for (const auto& network_interface : all_network_interfaces_)
  {
    if (network_interface->selected)
    {
      std::cout << "    X   ";
    }
    else
    {
      std::cout << "        ";
    }
    std::cout << "   " << network_interface->ui_index << "   " << network_interface->name << " ("
              << network_interface->addr_string << ")\n";
  }
}  // PrintNics

bool NetworkSelect::IsAnyNicSelected() const
{
  for (const auto& network_interface : all_network_interfaces_)
  {
    if (network_interface->selected)
    {
      return true;
    }
  }
  return false;
}  // IsAnyNicSelected

void NetworkSelect::SelectNics()
{
  int input = -1;
  while (true)
  {
    if (input != '\n')
    {
      PrintNics();
      std::cout << "Type index letter to select / deselect a network interface, type 0 when finished\n";
    }
    input = getchar();
    if (input == '0')
    {
      if (IsAnyNicSelected())
      {
        return;
      }
      std::cout << "Please select at least one network interface\n\n";
      continue;
    }
    if (input != '\n')
    {
      bool found = false;
      for (const auto& network_interface : all_network_interfaces_)
      {
        if (input == network_interface->ui_index)
        {
          found                       = true;
          network_interface->selected = !network_interface->selected;
        }
      }
      if (!found)
      {
        std::cout << "Invalid input.\n";
      }
    }
  }
}  // SelectNics

std::vector<SacnMcastInterface> NetworkSelect::GetMcastInterfaces() const
{
  std::vector<SacnMcastInterface> interfaces;
  for (const auto& network_interface : all_network_interfaces_)
  {
    if (network_interface && network_interface->selected)
    {
      SacnMcastInterface iface = {
          {static_cast<etcpal_iptype_t>(network_interface->addr.type()), network_interface->os_index.value()},
          kEtcPalErrOk};
      interfaces.push_back(iface);
    }
  }
  return interfaces;
}  // GetMcastInterfaces
