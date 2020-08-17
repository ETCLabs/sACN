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

typedef struct SourceState
{
  source_id_t handle;
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
  EtcPalRbTree source_handle_lookup;

  const SacnDmxMergerConfig* config;
  uint8_t winning_priorities[DMX_ADDRESS_COUNT];
} MergerState;

IntHandleManager merger_handle_mgr;
EtcPalRbTree mergers;

// TODO: DO WE NEED THIS FILE??
int merger_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
int source_handle_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);

EtcPalRbNode* dmx_merger_rb_node_alloc_func();
void dmx_merger_rb_node_dealloc_func(EtcPalRbNode* node);

bool merger_handle_in_use(int handle_val, void* cookie);
bool source_handle_in_use(int handle_val, void* cookie);

void update_levels(MergerState* merger, SourceState* source, const uint8_t* new_values, uint16_t new_values_count);
void update_level(MergerState* merger, SourceState* source, uint16_t level_index, uint8_t level);
void update_per_address_priorities(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                                   uint16_t address_priorities_count);
void update_priority(MergerState* merger, SourceState* source, uint16_t priority_index, uint8_t priority);
void update_universe_priority(MergerState* merger, SourceState* source, uint8_t priority);
void merge_source(MergerState* merger, SourceState* source, uint16_t slot_index);
void deinit_merger_state(MergerState* state);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_DMX_MERGER_H_ */
