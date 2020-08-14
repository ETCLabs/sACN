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
#include "sacn/private/util.h"
#include "etcpal/rbtree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WinnerLookupKeys
{
  unsigned int slot_index;
  source_id_t owner;
  uint8_t level;
  uint8_t priority;
} WinnerLookupKeys;

typedef struct SourceState
{
  source_id_t handle;
  // TODO: Keep SacnDmxMergerSource arrays up-to-date.
  SacnDmxMergerSource source;
} SourceState;

typedef struct CidToSourceHandle
{
  EtcPalUuid cid;
  source_id_t handle;
} CidToSourceHandle;

typedef struct MergerState
{
  sacn_dmx_merger_t handle;

  IntHandleManager source_handle_mgr;

  EtcPalRbTree source_state_lookup;
  EtcPalRbTree winner_lookup;
  EtcPalRbTree source_handle_lookup;

  const SacnDmxMergerConfig* config;
} MergerState;

IntHandleManager merger_handle_mgr;
EtcPalRbTree mergers;

// TODO: DO WE NEED THIS FILE??
int merger_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
int winner_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
int source_handle_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);

EtcPalRbNode* dmx_merger_rb_node_alloc_func();
void dmx_merger_rb_node_dealloc_func(EtcPalRbNode* node);

bool merger_handle_in_use(int handle_val, void* cookie);
bool source_handle_in_use(int handle_val, void* cookie);

etcpal_error_t update_levels(MergerState* merger, SourceState* source, const uint8_t* new_values,
                             size_t new_values_count);
etcpal_error_t update_level(MergerState* merger, SourceState* source, unsigned int level_index, uint8_t level);
etcpal_error_t update_per_address_priorities(MergerState* merger, SourceState* source,
                                             const uint8_t* address_priorities, size_t address_priorities_count);
etcpal_error_t update_priority(MergerState* merger, SourceState* source, unsigned int priority_index, uint8_t priority);
etcpal_error_t update_universe_priority(MergerState* merger, SourceState* source, uint8_t priority);
WinnerLookupKeys get_winner_lookup_source_lower_bound_keys(unsigned int slot_index, SourceState* source);
WinnerLookupKeys get_winner_lookup_source_upper_bound_keys(unsigned int slot_index, SourceState* source);
WinnerLookupKeys get_winner_lookup_slot_lower_bound_keys(unsigned int slot_index);
WinnerLookupKeys get_winner_lookup_slot_upper_bound_keys(unsigned int slot_index);
etcpal_error_t update_winner_lookup(MergerState* merger, WinnerLookupKeys* current_keys_to_free,
                                    const WinnerLookupKeys* new_keys_to_copy);
WinnerLookupKeys* get_current_keys(MergerState* merger, SourceState* source, unsigned int slot_index);
bool keys_valid(const WinnerLookupKeys* keys);
void update_merge(MergerState* merger, unsigned int slot_index);
void deinit_merger_state(MergerState* state);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_DMX_MERGER_H_ */
