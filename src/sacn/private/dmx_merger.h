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
 * @file sacn/private/dmx_merger.h
 * @brief Private constants, types, and function declarations for the
 *        @ref sacn_dmx_merger "sACN DMX Merger" module.
 */

#ifndef SACN_PRIVATE_DMX_MERGER_H_
#define SACN_PRIVATE_DMX_MERGER_H_

#include <stdbool.h>
#include <stdint.h>
#include "sacn/dmx_merger.h"
#include "sacn/private/util.h"
#include "etcpal/handle_manager.h"
#include "etcpal/rbtree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SourceState
{
  sacn_dmx_merger_source_t handle;  // This must be the first struct member.
  SacnDmxMergerSource source;
  size_t pap_count;
  bool universe_priority_uninitialized;
} SourceState;

typedef struct MergerState
{
  sacn_dmx_merger_t handle;  // This must be the first struct member.
  IntHandleManager source_handle_mgr;
  EtcPalRbTree source_state_lookup;
  SacnDmxMergerConfig config;

#if !SACN_DMX_MERGER_DISABLE_INTERNAL_PAP_BUFFER
  /* If a merger config is passed in with per_address_priorities set to NULL, config.per_address_priorities will be set
   * to point to this so that the winning priorities can still be tracked. */
  uint8_t pap_internal[DMX_ADDRESS_COUNT];
#endif

#if !SACN_DMX_MERGER_DISABLE_INTERNAL_OWNER_BUFFER
  /* If a merger config is passed in with owners set to NULL, config.owners will be set to point to this so that the
   * merge winners can still be tracked. */
  sacn_dmx_merger_source_t owners_internal[DMX_ADDRESS_COUNT];
#endif
} MergerState;

etcpal_error_t sacn_dmx_merger_init();
void sacn_dmx_merger_deinit(void);

etcpal_error_t lookup_state(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, MergerState** merger_state,
                            SourceState** source_state);
size_t get_number_of_mergers();

etcpal_error_t create_sacn_dmx_merger(const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle);
etcpal_error_t destroy_sacn_dmx_merger(sacn_dmx_merger_t handle);
etcpal_error_t remove_sacn_dmx_merger_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source);
etcpal_error_t add_sacn_dmx_merger_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t* handle);
etcpal_error_t add_sacn_dmx_merger_source_with_handle(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t handle_to_use);

etcpal_error_t update_sacn_dmx_merger_levels(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                             const uint8_t* new_levels, size_t new_levels_count);
etcpal_error_t update_sacn_dmx_merger_pap(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, const uint8_t* pap,
                                          size_t pap_count);
etcpal_error_t update_sacn_dmx_merger_universe_priority(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                                        uint8_t universe_priority);
etcpal_error_t remove_sacn_dmx_merger_pap(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_DMX_MERGER_H_ */
