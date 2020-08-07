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

/*!
 * \file sacn/private/dmx_merger.h
 * \brief Private constants, types, and function declarations for the
 *        \ref sacn_dmx_merger "sACN DMX Merger" module.
 */

#ifndef SACN_PRIVATE_DMX_MERGER_H_
#define SACN_PRIVATE_DMX_MERGER_H_

#include <stdbool.h>
#include <stdint.h>
#include "../../../include/sacn/dmx_merger.h"
#include "etcpal/rbtree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SourceState
{
  source_id_t handle;
  SacnDmxMergerSource source;
} SourceState;

typedef struct WinnerLookupKeys
{
  source_id_t owner;
  uint8_t level;
  uint8_t priority;
} WinnerLookupKeys;

typedef struct MergerState
{
  sacn_dmx_merger_t handle;

  EtcPalRbTree sources;
  EtcPalRbTree winner_lookup[DMX_ADDRESS_COUNT];
} MergerState;

EtcPalRbTree mergers;

// TODO: DO WE NEED THIS FILE??
etcpal_error_t update_levels(MergerState* merger, SourceState* source, const uint8_t* new_values,
                             size_t new_values_count);
etcpal_error_t update_level(MergerState* merger, SourceState* source, unsigned int level_index, uint8_t level);
etcpal_error_t update_level_count(MergerState* merger, SourceState* source, size_t new_values_count);
etcpal_error_t update_per_address_priorities(MergerState* merger, SourceState* source,
                                             const uint8_t* address_priorities, size_t address_priorities_count);
etcpal_error_t update_universe_priority(MergerState* merger, SourceState* source, uint8_t priority);
bool merger_is_added(sacn_dmx_merger_t handle);
WinnerLookupKeys get_winner_lookup_keys(SourceState* source, unsigned int slot_index);
etcpal_error_t update_winner_lookup(MergerState* merger, unsigned int slot_index, const WinnerLookupKeys* current_keys,
                                    const WinnerLookupKeys* new_keys);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_DMX_MERGER_H_ */
