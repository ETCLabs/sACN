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

#include "sacn/private/util.h"

bool supports_ipv4(sacn_ip_support_t support)
{
  return ((support == kSacnIpV4Only) || (support == kSacnIpV4AndIpV6));
}

bool supports_ipv6(sacn_ip_support_t support)
{
  return ((support == kSacnIpV6Only) || (support == kSacnIpV4AndIpV6));
}
