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

#include "network_select.h"
#include <iostream>

NetworkSelect::NetworkSelect()
{
}
  
void NetworkSelect::InitializeNics(void)
{
  size_t num_netints = 4;  // Start with estimate which eventually has the actual number written to it
  EtcPalNetintInfo* netints = (EtcPalNetintInfo*)calloc(num_netints, sizeof(EtcPalNetintInfo));
  while (etcpal_netint_get_interfaces(netints, &num_netints) == kEtcPalErrBufSize)
  {
    netints = (EtcPalNetintInfo*)realloc(netints, num_netints * sizeof(EtcPalNetintInfo));
  }

  char char_index = 'a';
  for (EtcPalNetintInfo *netint = netints;
       netint < netints + num_netints;
       ++netint)
  {
    if ((strlen(netint->friendly_name) > 0) && (netint->addr.type != kEtcPalIpTypeInvalid))
    {
      etcpal::IpAddr ip_address = netint->addr;
      std::unique_ptr<EtcPalNetintInfoSelect> network_interface = std::make_unique<EtcPalNetintInfoSelect>();
      network_interface->selected = false;
      network_interface->ui_index = char_index++;
      network_interface->os_index = netint->index;
      network_interface->addr = netint->addr;
      network_interface->addr_string = ip_address.ToString();
      network_interface->name = netint->friendly_name;
      all_network_interfaces_.push_back(std::move(network_interface));
    }
  }
  free(netints);
} // InitializeNics

void NetworkSelect::PrintNics(void) const
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
} // PrintNics

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
} // IsAnyNicSelected

void NetworkSelect::SelectNics(void)
{
  int ch = -1;
  while (true)
  {
    if (ch != '\n')
    {
      PrintNics();
      std::cout << "Type index letter to select / deselect a network interface, type 0 when finished\n";
    }
    ch = getchar();
    if (ch == '0')
    {
      if (IsAnyNicSelected())
      {
        return;
      }
      std::cout << "Please select at least one network interface\n\n";
      continue;
    }
    if (ch != '\n')
    {
      bool found = false;
      for (const auto& network_interface : all_network_interfaces_)
      {
        if (ch == network_interface->ui_index)
        {
          found = true;
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

std::vector<SacnMcastInterface> NetworkSelect::GetMcastInterfaces(void) const
{
  std::vector<SacnMcastInterface> interfaces;
  for (const auto& network_interface : all_network_interfaces_)
  {
    if (network_interface->selected)
    {
      SacnMcastInterface i = {
          {static_cast<etcpal_iptype_t>(network_interface->addr.type()), network_interface->os_index}, kEtcPalErrOk};
      interfaces.push_back(i);
    }
  }
  return interfaces;
} // GetMcastInterfaces
