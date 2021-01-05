/******************************************************************************
 * Copyright 2020 ETC Inc.
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

/**
 * @file sacn/private/util.h
 * @brief Utilities used internally by the sACN library
 */

#ifndef SACN_PRIVATE_UTIL_H_
#define SACN_PRIVATE_UTIL_H_

#include <stdbool.h>
#include "sacn/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*HandleValueInUseFunction)(int handle_val, void* cookie);

/* Manage generic integer handle values.
 *
 * This struct and the accompanying functions are a utility to manage handing out integer handles
 * to resources. It first assigns monotonically-increasing positive values starting at 0 to handles;
 * after wraparound, it uses the value_in_use function to find holes where new handle values can be
 * assigned.
 */
typedef struct IntHandleManager
{
  int next_handle;
  /* Optimize the handle generation algorithm by tracking whether the handle value has wrapped around. */
  bool handle_has_wrapped_around;
  /* Function pointer to determine if a handle value is currently in use. Used only after the handle
   * value has wrapped around once. */
  HandleValueInUseFunction value_in_use;
  /* A cookie passed to the value in use function. */
  void* cookie;
} IntHandleManager;

void init_int_handle_manager(IntHandleManager* manager, HandleValueInUseFunction value_in_use_func, void* cookie);
int get_next_int_handle(IntHandleManager* manager, int max);

bool supports_ipv4(sacn_ip_support_t support);
bool supports_ipv6(sacn_ip_support_t support);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_UTIL_H_ */
