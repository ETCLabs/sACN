/******************************************************************************
 * Copyright 2021 ETC Inc.
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

#include "sacn/private/util.h"

#include <string.h>

/* IntHandleManager functions */

void init_int_handle_manager(IntHandleManager* manager, HandleValueInUseFunction value_in_use_func, void* cookie)
{
  manager->next_handle = 0;
  manager->handle_has_wrapped_around = false;
  manager->value_in_use = value_in_use_func;
  manager->cookie = cookie;
}

/* Max determines a custom wrap-around point. If max is negative, it will be ignored. */
int get_next_int_handle(IntHandleManager* manager, int max)
{
  int new_handle = manager->next_handle;

  ++manager->next_handle;

  if ((manager->next_handle < 0) || ((max >= 0) && (manager->next_handle > max)))
  {
    manager->next_handle = 0;
    manager->handle_has_wrapped_around = true;
  }

  // Optimization - keep track of whether the handle counter has wrapped around.
  // If not, we don't need to check if the new handle is in use.
  if (manager->handle_has_wrapped_around)
  {
    // We have wrapped around at least once, we need to check for handles in use
    int original = new_handle;
    while (manager->value_in_use(new_handle, manager->cookie))
    {
      if (manager->next_handle == original)
      {
        // Incredibly unlikely case of all handles used
        new_handle = -1;
        break;
      }
      new_handle = manager->next_handle;
      ++manager->next_handle;
      if ((manager->next_handle < 0) || ((max >= 0) && (manager->next_handle > max)))
        manager->next_handle = 0;
    }
  }

  return new_handle;
}

bool supports_ipv4(sacn_ip_support_t support)
{
  return ((support == kSacnIpV4Only) || (support == kSacnIpV4AndIpV6));
}

bool supports_ipv6(sacn_ip_support_t support)
{
  return ((support == kSacnIpV6Only) || (support == kSacnIpV4AndIpV6));
}
