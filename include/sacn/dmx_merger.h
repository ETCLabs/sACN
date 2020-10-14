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
#include "etcpal/uuid.h"
#include "sacn/common.h"
#include "sacn/receiver.h"

/**
 * @defgroup sacn_dmx_merger sACN DMX Merger
 * @ingroup sACN
 * @brief The sACN DMX Merger API
 *
 * This API provides a software merger for buffers containing DMX512-A start code 0 packets.
 * It also uses buffers containing DMX512-A start code 0xdd packets to support per-address priority.
 *
 * While this API is used to easily merge the outputs from the sACN Receiver API, it can also be used
 * to merge your own DMX sources together, even in combination with the sources received via sACN.
 *
 * When asked to calculate the merge, the merger will evaluate the current source
 * buffers and update two result buffers:
 *  - 512 bytes for the merged data values (i.e. "winning level").  These are calculated by using
 *     a Highest-Level-Takes-Precedence(HTP) algorithm for all sources that share the highest
 *     per-address priority.
 *  - 512 source identifiers (i.e. "winning source") to indicate which source was considered the
 *     source of the merged data value, or that no source currently owns this address.
 *
 * This API is thread-safe.
 *
 * Usage:
 * @code
 * // Initialize sACN.
 * EtcPalLogParams log_params = ETCPAL_LOG_PARAMS_INIT;
 * // Init log params here...
 *
 * sacn_init(&log_params);
 *
 * // These buffers are updated on each merger call with the merge results.
 * // They must be valid as long as the merger is using them.
 * uint8_t slots[DMX_ADDRESS_COUNT];
 * sacn_source_id_t slot_owners[DMX_ADDRESS_COUNT];
 *
 * // Merger configuration used for the initialization of each merger:
 * SacnDmxMergerConfig merger_config = SACN_DMX_MERGER_CONFIG_INIT;
 * merger_config.slots = slots;
 * merger_config.slot_owners = slot_owners;
 * merger_config.source_count_max = SACN_RECEIVER_INFINITE_SOURCES;
 *
 * // A merger has a handle, as do each of its sources. Source CIDs are tracked as well.
 * sacn_dmx_merger_t merger_handle;
 * sacn_source_id_t source_1_handle, source_2_handle;
 * EtcPalUuid source_1_cid, source_2_cid;
 * // Initialize CIDs here...
 *
 * // Make sure to check/handle the etcpal_error_t return values (this is omitted in this example).
 *
 * // Initialize a merger and two sources, getting their handles in return.
 * sacn_dmx_merger_create(&merger_config, &merger_handle);
 *
 * sacn_dmx_merger_add_source(merger_handle, &source_1_cid, &source_1_handle);
 * sacn_dmx_merger_add_source(merger_handle, &source_2_cid, &source_2_handle);
 *
 * // Input data for merging:
 * uint8_t levels[DMX_ADDRESS_COUNT];
 * uint8_t paps[DMX_ADDRESS_COUNT];
 * uint8_t universe_priority;
 * // Initialize levels, paps, and universe_priority here...
 *
 * // Levels and PAPs can be merged separately:
 * sacn_dmx_merger_update_source_data(merger_handle, source_1_handle, universe_priority, levels, DMX_ADDRESS_COUNT,
 *                                    NULL, 0);
 * sacn_dmx_merger_update_source_data(merger_handle, source_1_handle, universe_priority, NULL, 0, paps,
 *                                    DMX_ADDRESS_COUNT);
 *
 * // Or together in one call:
 * sacn_dmx_merger_update_source_data(merger_handle, source_2_handle, universe_priority, levels, DMX_ADDRESS_COUNT,
 *                                    paps, DMX_ADDRESS_COUNT);
 *
 * // Or, if this is within a sACN receiver callback, use sacn_dmx_merger_update_source_from_sacn:
 * SacnHeaderData header;
 * uint8_t pdata[DMX_ADDRESS_COUNT];
 * // Assuming header and pdata are initialized.
 *
 * sacn_dmx_merger_update_source_from_sacn(merger_handle, &header, pdata);
 *
 * // PAP can also be removed. Here, source 1 reverts to universe_priority:
 * sacn_dmx_merger_stop_source_per_address_priority(merger_handle, source_1_handle);
 *
 * // The read-only state of each source can be obtained as well.
 * const SacnDmxMergerSource* source_1_state = sacn_dmx_merger_get_source(merger_handle, source_1_handle);
 * const SacnDmxMergerSource* source_2_state = sacn_dmx_merger_get_source(merger_handle, source_2_handle);
 *
 * // Do something with the merge results (slots and slot_owners)...
 *
 * // Sources can be removed individually:
 * sacn_dmx_merger_remove_source(merger_handle, source_1_handle);
 * sacn_dmx_merger_remove_source(merger_handle, source_2_handle);
 *
 * // However, when each merger is destroyed, all of its sources are removed along with it:
 * sacn_dmx_merger_destroy(merger_handle);
 *
 * // Or, if sACN is deinitialized, all of the mergers are destroyed automatically:
 * sacn_deinit();
 * @endcode
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

/** The sources on a merger have a short id that is used in the owned values, rather than a UUID.*/
typedef uint16_t sacn_source_id_t;

/** An invalid source id handle value. */
#define SACN_DMX_MERGER_SOURCE_INVALID ((sacn_source_id_t) -1)

/** A set of configuration information for a merger instance. */
typedef struct SacnDmxMergerConfig
{
  /** The maximum number of sources this merger will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
      This parameter is ignored when configured to use static memory -- #SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER is used
      instead.*/
  size_t source_count_max;

  /** Buffer of #DMX_ADDRESS_COUNT levels that this library keeps up to date as it merges.
      Memory is owned by the application.*/
  uint8_t* slots;

  /** Buffer of #DMX_ADDRESS_COUNT source IDs that indicate the current winner of the merge for
      that slot, or #DMX_MERGER_SOURCE_INVALID to indicate that no source is providing values for that slot.
      You can use SACN_DMX_MERGER_SOURCE_IS_VALID() if you don't want to look at the slot_owners directly.
      Memory is owned by the application.*/
  sacn_source_id_t* slot_owners;

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
#define SACN_DMX_MERGER_CONFIG_INIT \
  {                                 \
    0, NULL, NULL                   \
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
  /** The UUID (e.g. sACN CID) of the DMX source. */
  EtcPalUuid cid;

  /** The DMX data values (0 - 255). */
  uint8_t values[DMX_ADDRESS_COUNT];

  /** Some sources don't send all 512 values, so here's how much of values to use.*/
  size_t valid_value_count;

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

etcpal_error_t sacn_dmx_merger_add_source(sacn_dmx_merger_t merger, const EtcPalUuid* source_cid,
                                          sacn_source_id_t* source_id);
etcpal_error_t sacn_dmx_merger_remove_source(sacn_dmx_merger_t merger, sacn_source_id_t source);
sacn_source_id_t sacn_dmx_merger_get_id(sacn_dmx_merger_t merger, const EtcPalUuid* source_cid);
const SacnDmxMergerSource* sacn_dmx_merger_get_source(sacn_dmx_merger_t merger, sacn_source_id_t source);
etcpal_error_t sacn_dmx_merger_update_source_data(sacn_dmx_merger_t merger, sacn_source_id_t source, uint8_t priority,
                                                  const uint8_t* new_values, size_t new_values_count,
                                                  const uint8_t* address_priorities, size_t address_priorities_count);
etcpal_error_t sacn_dmx_merger_update_source_from_sacn(sacn_dmx_merger_t merger, const SacnHeaderData* header,
                                                       const uint8_t* pdata);
etcpal_error_t sacn_dmx_merger_stop_source_per_address_priority(sacn_dmx_merger_t merger, sacn_source_id_t source);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_DMX_MERGER_H_ */
