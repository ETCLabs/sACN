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
#include "etcpal/rbtree.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SourceState
{
  sacn_source_id_t handle;  // This must be the first struct member.
  SacnDmxMergerSource source;
} SourceState;

typedef struct CidHandleMapping
{
  EtcPalUuid cid;  // This must be the first struct member.
  sacn_source_id_t handle;
} CidHandleMapping;

typedef struct MergerState
{
  sacn_dmx_merger_t handle;  // This must be the first struct member.

  IntHandleManager source_handle_mgr;

  EtcPalRbTree source_state_lookup;
  EtcPalRbTree source_handle_lookup;

  size_t source_count_max;
  uint8_t* slots;
  sacn_source_id_t* slot_owners;

  uint8_t winning_priorities[DMX_ADDRESS_COUNT];
} MergerState;

etcpal_error_t sacn_dmx_merger_init();
void sacn_dmx_merger_deinit(void);

MergerState* find_merger_state(sacn_dmx_merger_t handle);
size_t get_number_of_mergers();

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_DMX_MERGER_H_ */
