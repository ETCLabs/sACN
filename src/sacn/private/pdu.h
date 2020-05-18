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

#ifndef SACN_PRIVATE_PDU_H_
#define SACN_PRIVATE_PDU_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sacn/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VECTOR_E131_DATA_PACKET 0x00000002u

#define SACN_OPTVAL_PREVIEW 0x80u
#define SACN_OPTVAL_TERMINATED 0x40u
#define SACN_OPTVAL_FORCE_SYNC 0x20u

#define SACN_STARTCODE_DMX 0x00u
#if SACN_ETC_PRIORITY_EXTENSION
#define SACN_STARTCODE_PRIORITY 0xddu
#endif

#define SACN_DATA_HEADER_SIZE 126
#define SACN_PRI_OFFSET 108
#define SACN_SEQ_OFFSET 111
#define SACN_OPTS_OFFSET 112

#define SET_SEQUENCE(bufptr, seq) (bufptr[SACN_SEQ_OFFSET] = seq)
#define SET_TERMINATED_OPT(bufptr, terminated)                        \
  do                                                                  \
  {                                                                   \
    if (terminated)                                                   \
      bufptr[SACN_OPTS_OFFSET] |= SACN_OPTVAL_TERMINATED;             \
    else                                                              \
      bufptr[SACN_OPTS_OFFSET] &= (uint8_t)(~SACN_OPTVAL_TERMINATED); \
  } while (0)
#define TERMINATED_OPT_SET(bufptr) (bufptr[SACN_OPTS_OFFSET] & SACN_OPTVAL_TERMINATED)
#define SET_PREVIEW_OPT(bufptr, preview)                           \
  do                                                               \
  {                                                                \
    if (preview)                                                   \
      bufptr[SACN_OPTS_OFFSET] |= SACN_OPTVAL_PREVIEW;             \
    else                                                           \
      bufptr[SACN_OPTS_OFFSET] &= (uint8_t)(~SACN_OPTVAL_PREVIEW); \
  } while (0)
#define SET_PRIORITY(bufptr, priority) ((void)(bufptr[SACN_PRI_OFFSET] = priority));

bool parse_sacn_data_packet(const uint8_t* buf, size_t buflen, SacnHeaderData* header, uint8_t* seq, bool* terminated,
                            const uint8_t** pdata);
bool parse_draft_sacn_data_packet(const uint8_t* buf, size_t buflen, SacnHeaderData* header, uint8_t* seq,
                                  bool* terminated, const uint8_t** pdata);
size_t pack_sacn_data_header(uint8_t* buf, const EtcPalUuid* source_cid, const char* source_name, uint8_t priority,
                             bool preview, uint16_t universe_id, uint8_t start_code, uint16_t slot_count);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_PDU_H_ */
