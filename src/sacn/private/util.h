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

bool supports_ipv4(sacn_ip_support_t support);
bool supports_ipv6(sacn_ip_support_t support);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_UTIL_H_ */
