/******************************************************************************
 * Copyright 2021 ETC Inc.
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
 * @file sacn/dmx_merger.h
 * @brief sACN DMX Merger API definitions
 *
 * Functions and definitions for the @ref sacn_dmx_merger "sACN DMX Merger API" are contained in this
 * header.
 */

#ifndef SACN_DMX_MERGER_H_
#define SACN_DMX_MERGER_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "sacn/common.h"
#include "sacn/receiver.h"

/**
 * @defgroup sacn_dmx_merger sACN DMX Merger
 * @ingroup sACN
 * @brief The sACN DMX Merger API; see @ref using_dmx_merger.
 *
 * This API provides a software merger for buffers containing DMX512-A start code 0 packets.
 * It also uses buffers containing DMX512-A start code 0xdd packets to support per-address priority.
 *
 * When asked to calculate the merge, the merger will evaluate the current source
 * buffers and update two result buffers:
 *  - 512 bytes for the merged data levels (i.e. "winning level").  These are calculated by using
 *     a Highest-Level-Takes-Precedence(HTP) algorithm for all sources that share the highest
 *     per-address priority.
 *  - 512 source identifiers (i.e. "winning source") to indicate which source was considered the
 *     source of the merged data level, or that no source currently owns this address.
 *
 * This API is thread-safe.
 *
 * See @ref using_dmx_merger for a detailed description of how to use this API.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Each merger has a handle associated with it.*/
typedef int sacn_dmx_merger_t;

/** An invalid sACN merger handle value. */
#define SACN_DMX_MERGER_INVALID -1

/** A source handle used by the DMX merger, could represent a remote source or another logical source (e.g. a local DMX
 * port). */
typedef uint16_t sacn_dmx_merger_source_t;

/** An invalid DMX merger source handle value. */
#define SACN_DMX_MERGER_SOURCE_INVALID ((sacn_dmx_merger_source_t)-1)

/** A set of configuration information for a merger instance. */
typedef struct SacnDmxMergerConfig
{
  /********* Required values **********/

  /** Buffer of #DMX_ADDRESS_COUNT levels that this library keeps up to date as it merges.  Slots that are not sourced
      are set to 0.
      Memory is owned by the application, but while this merger exists the application must not modify this buffer
      directly!  Doing so would affect the results of the merge.*/
  uint8_t* slots;

  /********* Optional values **********/

  /** Buffer of #DMX_ADDRESS_COUNT per-address priorities for each winning slot. This is used if the merge results need
      to be sent over sACN. Otherwise this can just be set to NULL. If a source with a universe priority of 0 wins, that
      priority is converted to 1. If there is no winner for a slot, then a per-address priority of 0 is used to show
      that there is no source for that slot.
      Memory is owned by the application.*/
  uint8_t* per_address_priorities;

  /** Buffer of #DMX_ADDRESS_COUNT source IDs that indicate the current winner of the merge for that slot, or
      #SACN_DMX_MERGER_SOURCE_INVALID to indicate that there is no winner for that slot. This is used if you
      need to know the source of each slot. If you only need to know whether or not a slot is sourced, set this to NULL
      and use per_address_priorities (which has half the memory footprint) to check if the slot has a priority of 0 (not
      sourced).
      Memory is owned by the application.*/
  sacn_dmx_merger_source_t* slot_owners;

  /** The maximum number of sources this merger will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
      This parameter is ignored when configured to use static memory -- #SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER is used
      instead.*/
  int source_count_max;

} SacnDmxMergerConfig;

/**
 * @brief An initializer for an SacnDmxMergerConfig struct.
 *
 * Usage:
 * @code
 * // Create the struct
 * SacnDmxMergerConfig merger_config = SACN_DMX_MERGER_CONFIG_INIT;
 * // Now fill in the members of the struct
 * @endcode
 *
 */
#define SACN_DMX_MERGER_CONFIG_INIT                  \
  {                                                  \
    NULL, NULL, NULL, SACN_RECEIVER_INFINITE_SOURCES \
  }

/**
 * @brief Utility to see if a slot_owner is valid.
 *
 * Given a buffer of slot_owners, evaluate to true if the slot is != DMX_MERGER_SOURCE_INVALID.
 *
 */
#define SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners_array, slot_index) \
  (slot_owners_array[slot_index] != SACN_DMX_MERGER_SOURCE_INVALID)

/** The current input data for a single source of the merge.  This is exposed only for informational purposes, as the
    application calls a variant of sacn_dmx_merger_update_source to do the actual update. */
typedef struct SacnDmxMergerSource
{
  /** The merger's ID for the DMX source. */
  sacn_dmx_merger_source_t id;

  /** The DMX NULL start code data (0 - 255). */
  uint8_t levels[DMX_ADDRESS_COUNT];

  /** Some sources don't send all 512 levels, so here's how much of levels to use.*/
  size_t valid_level_count;

  /** The sACN per-universe priority (0 - 200). */
  uint8_t universe_priority;

  /** Whether or not the address_priority buffer is valid. */
  bool address_priority_valid;

  /** The sACN per-address (startcode 0xdd) priority (1-255, 0 means not sourced).
      If the source does not have per-address priority, then address_priority_valid will be false, and this array should
      be ignored. */
  uint8_t address_priority[DMX_ADDRESS_COUNT];

} SacnDmxMergerSource;

etcpal_error_t sacn_dmx_merger_create(const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle);
etcpal_error_t sacn_dmx_merger_destroy(sacn_dmx_merger_t handle);

etcpal_error_t sacn_dmx_merger_add_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t* source_id);
etcpal_error_t sacn_dmx_merger_remove_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source);
const SacnDmxMergerSource* sacn_dmx_merger_get_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source);

etcpal_error_t sacn_dmx_merger_update_levels(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                             const uint8_t* new_levels, size_t new_levels_count);
etcpal_error_t sacn_dmx_merger_update_paps(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, const uint8_t* paps,
                                           size_t paps_count);
etcpal_error_t sacn_dmx_merger_update_universe_priority(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                                        uint8_t universe_priority);
etcpal_error_t sacn_dmx_merger_remove_paps(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_DMX_MERGER_H_ */
