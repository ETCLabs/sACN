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
 * \file sacn/private/merge.h
 * \brief Private constants, types, and function declarations for the
 *        \ref dmx_merger "Dmx Merger" module.
 */

#ifndef SACN_PRIVATE_MERGE_H_
#define SACN_PRIVATE_MERGE_H_

#include <stdbool.h>
#include <stdint.h>
#include "../../../incl"

#ifdef __cplusplus
extern "C" {
#endif

/*The information for a source on a universe to be merged. */
typedef struct DmxMergerSource
{
  /*TODO: CID, Source ID*/

  /* The DMX data values (0 - 255) */
  uint8_t values[DMX_MERGER_SLOT_COUNT];

  /* The per-universe priority (0 - 255)*/
  uint8_t universe_priority;

  /* The per-address priority (1-255, 0 means not sourced)*/
  uint8_t address_priority[DMX_MERGER_SLOT_COUNT];

  /* If >= 0, this is the current number of channels to compare.
     Acts as a shortcut so we don't have to process all 512 values.*/
  int address_count;
} DmxMergerSource;

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_MERGE_H_ */
