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
 * \file sacn/dmx_merger.h
 * \brief sACN DMX Merger API definitions
 *
 * Functions and definitions for the \ref sacn_dmx_merger "sACN DMX Merger API" are contained in this
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

/*!
 * \defgroup sacn_dmx_merger sACN DMX Merger
 * \ingroup sACN
 * \brief The sACN DMX Merger API
 *
 * This API provides a software merger for buffers containing DMX512-A start code 0 packets.
 * It also uses buffers containing DMX512-A start code 0xdd packets to support per-address priority.
 *
 * While this API is used to easily merge the outputs from the sACN Receiver API, it can also be used
 * to merge your own DMX sources together, even in combination with the sources received via sACN.
 *
 * When asked to calculate the merge, the merger shall evaluate the current source
 * buffers and update two result buffers:
 *  - 512 bytes for the merged data values (i.e. "winning level").  These are calculated by using
 *     a Highest-Level-Takes-Precedence(HTP) algorithm for all sources that share the highest
 *     per-address priority.
 *  - 512 source identifiers (i.e. "winning source") to indicate which source was considered the
 *     source of the merged data value, or that no source currently owns this address.
 *
 * TODO: Add sample usage
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! Each merger has a handle associated with it.*/
typedef int sacn_dmx_merger_t;

/*! The sources on a merger have a short id that is used in the owned values, rather than a UUID.*/
typedef uint16_t source_id_t;

/*! An invalid source id handle value. */
#define SACN_DMX_MERGER_SOURCE_INVALID -1

/*! A set of configuration information for a merger instance. */
typedef struct SacnDmxMergerConfig
{
  /*! The maximum number of sources this merger will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
      This parameter is ignored when configured to use static memory -- #SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER is used
      instead.*/
  size_t source_count_max;

  /*! Buffer of #DMX_ADDRESS_COUNT levels that this library keeps up to date as it merges.
      Memory is owned by the application.*/
  uint8_t* slots;

  /*! Buffer of #DMX_ADDRESS_COUNT source IDs that indicate the current winner of the merge for
      that slot, or #DMX_MERGER_SOURCE_INVALID to indicate that no source is providing values for that slot.
      Memory is owned by the application.*/
  source_id_t* slot_owners;

} SacnDmxMergerConfig;

/*!
 * \brief An initializer for an SacnDmxMergerConfig struct.
 *
 * Usage:
 * \code
 * // Create the struct
 * SacnDmxMergerConfig merger_config = SACN_DMX_MERGER_CONFIG_INIT;
 * // Now fill in the members of the struct
 * \endcode
 *
 */
#define SACN_DMX_MERGER_CONFIG_INIT \
  {                                 \
    0, NULL, NULL                   \
  }

/*! The current input data for a single source of the merge.  This is exposed only for informational purposes, as the
    application calls a variant of sacn_dmx_merger_update_source to do the actual update. */
typedef struct SacnDmxMergerSource
{
  /*! The UUID (e.g. sACN CID) of the DMX source. */
  EtcPalUuid cid;

  /*! The DMX data values (0 - 255). */
  uint8_t values[DMX_ADDRESS_COUNT];

  /*! Some sources don't send all 512 values, so here's how much of values to use.*/
  size_t valid_value_count;

  /*! The sACN per-universe priority (0 - 255). */
  uint8_t universe_priority;

  /*! Whether or not the address_priority buffer is valid. */
  bool address_priority_valid;

  /*! The sACN per-address (startcode 0xdd) priority (1-255, 0 means not sourced).
      If the source does not */
  uint8_t address_priority[DMX_ADDRESS_COUNT];

} SacnDmxMergerSource;

etcpal_error_t sacn_dmx_merger_init();
void sacn_dmx_merger_deinit(void);

etcpal_error_t sacn_dmx_merger_create(const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle);
etcpal_error_t sacn_dmx_merger_destroy(sacn_dmx_merger_t handle);

etcpal_error_t sacn_dmx_merger_add_source(sacn_dmx_merger_t merger, const EtcPalUuid* source_cid,
                                          source_id_t* source_id);
etcpal_error_t sacn_dmx_merger_remove_source(sacn_dmx_merger_t merger, source_id_t source);
source_id_t sacn_dmx_merger_get_id(sacn_dmx_merger_t merger, const EtcPalUuid* source_cid);
const SacnDmxMergerSource* sacn_dmx_merger_get_source(sacn_dmx_merger_t merger, source_id_t source);
etcpal_error_t sacn_dmx_merger_update_source_data(sacn_dmx_merger_t merger, source_id_t source,
                                                  const uint8_t* new_values, size_t new_values_count, uint8_t priority,
                                                  const uint8_t* address_priorities, size_t address_priorities_count);
// TODO: If Receiver API changes to notify both values and per-address priority data in the same callback, this should
// change!!
etcpal_error_t sacn_dmx_merger_update_source_from_sacn(sacn_dmx_merger_t merger, const SacnHeaderData* header,
                                                       const uint8_t* pdata);
etcpal_error_t sacn_dmx_merger_stop_source_per_address_priority(sacn_dmx_merger_t merger, source_id_t source);

// TODO: Do we need this?
etcpal_error_t sacn_dmx_merger_recalculate(sacn_dmx_merger_t merger);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* SACN_DMX_MERGER_H_ */
