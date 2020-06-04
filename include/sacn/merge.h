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
 * \file sacn/merge.h
 * \brief DMX Merge API definitions
 *
 * Functions and definitions for the \ref dmx_merger "DMX Merger API" are contained in this
 * header.
 */

#ifndef DMX_MERGER_H_
#define DMX_MERGER_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/uuid.h"

/*!
 * \defgroup dmx_merger DMX Merger
 * \ingroup sACN
 * \brief The DMX Merger API
 *
 * This API provides a software merger for buffers containing DMX512-A start code 0 packets.
 * It also uses buffers containing DMX512-A start code 0xdd packets to support per-address priority.
 *
 * When asked to calculate the merge of a universe, the merger shall evaluate the current source
 * buffers and update two result buffers:
 *   512 bytes for the merged data values (i.e. "winning level").  These are calculated by using
 *     a Highest-Level-Takes-Precedence(HTP) algorithm for all sources that share the highest
 *     per-address priority.
 *   512 source identifiers (i.e. "winning source") to indicate which source was considered the
 *     source of the merged data value, or that no source currently owns this address.
 *
 * \TODO: Add sample usage
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! The sources on a universe have a short id that is used in the owned values, rather than a UUID.*/
typedef uint16_t source_id_t;

/*! An invalid source id handle value. */
#define DMX_MERGER_SOURCE_INVALID -1

/*! The number of addresses in DMX universe. */
#define DMX_MERGER_SLOT_COUNT 512

/*! A set of configuration information for a universe to be merged. */
typedef struct DmxMergerUniverseConfig
{
  /*! Ignored when compiling with static memory support.
      This is the maximum number of sources to merge on a universe.  May be SACN_RECEIVER_INFINITE_SOURCES.*/
  int source_count_max;

  /*! Buffer of DMX_MERGER_SLOT_COUNT levels that this library keeps up to date as it merges.
      Memory is owned by the application.*/
  uint8_t* slots;

  /*! Buffer of DMX_MERGER_SLOT_COUNT source IDs that indicate the current winner of the merge for
      that slot, or DMX_MERGER_SOURCE_INVALID to indicate that no source is providing values for that slot.
      Memory is owned by the application.*/
  source_id_t* slot_owners;

} DmxMergerUniverseConfig;

/*! A default-value initializer for an SacnReceiverConfig struct. */
#define DMX_MERGER_UNIVERSE_CONFIG_DEFAULT_INIT         \
  {                                                     \
    0, NULL, NULL                                       \
  }

void dmx_merger_universe_config_init(DmxMergerUniverseConfig* config);

etcpal_error_t dmx_merger_init();
void dmx_merger_deinit(void);

//ADD .C SO WE CAN START DOCUMENTING>>>

//SHould we copy internally, or should we just use the buffers for update??  Look at existing api again?
//Add universe -- OR CREATE INSTANCE???  This is still just adding a universe.....
//Startup
//Shutdown
//Source handles, as 512 CIDS would be bad.
//Update universe/priority data -- immediate calc option?
//For each source on a universe, the application shall keep three buffers up to date:
//Util for filling/update buffers from SACN data (e.g. per-address & ownership, smaller DMX range & ownership, defaults)
//Add source
//Calculate
//WHO OWNS INPUT AND OUTPUT BUFFERS????  Arrays of CIDS?
//incomplete buffers? -- Use "Don't care in per-address prioirty"
//*The above data may be updated through API calls that process sACN packets.  The API will also allow the above buffers to be updated more directly in the case of non-sACN data.

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* DMX_MERGER_H_ */
