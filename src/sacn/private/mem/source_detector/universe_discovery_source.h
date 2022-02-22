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

#ifndef SACN_PRIVATE_UNIVERSE_DISCOVERY_SOURCE_MEM_H_
#define SACN_PRIVATE_UNIVERSE_DISCOVERY_SOURCE_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t init_universe_discovery_sources(void);
void deinit_universe_discovery_sources(void);

etcpal_error_t add_sacn_universe_discovery_source(const EtcPalUuid* cid, const char* name,
                                                  SacnUniverseDiscoverySource** source_state);
size_t replace_universe_discovery_universes(SacnUniverseDiscoverySource* source, size_t replace_start_index,
                                            const uint16_t* replacement_universes, size_t num_replacement_universes,
                                            size_t dynamic_universe_limit);
etcpal_error_t lookup_universe_discovery_source(sacn_remote_source_t handle,
                                                SacnUniverseDiscoverySource** source_state);
SacnUniverseDiscoverySource* get_first_universe_discovery_source(EtcPalRbIter* iterator);
SacnUniverseDiscoverySource* get_next_universe_discovery_source(EtcPalRbIter* iterator);
size_t get_num_universe_discovery_sources();
etcpal_error_t remove_sacn_universe_discovery_source(sacn_remote_source_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_UNIVERSE_DISCOVERY_SOURCE_MEM_H_ */
