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

#ifndef SACN_PRIVATE_UNICAST_DESTINATION_MEM_H_
#define SACN_PRIVATE_UNICAST_DESTINATION_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t add_sacn_unicast_dest(SacnSourceUniverse* universe, const EtcPalIpAddr* addr,
                                     SacnUnicastDestination** dest_state);
etcpal_error_t lookup_unicast_dest(SacnSourceUniverse* universe, const EtcPalIpAddr* addr,
                                   SacnUnicastDestination** unicast_dest);
void remove_sacn_unicast_dest(SacnSourceUniverse* universe, size_t index);

size_t get_unicast_dest_index(SacnSourceUniverse* universe, const EtcPalIpAddr* addr, bool* found);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_UNICAST_DESTINATION_MEM_H_ */
