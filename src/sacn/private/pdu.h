/******************************************************************************
 * Copyright 2022 ETC Inc.
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
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "etcpal/acn_rlp.h"
#include "etcpal/pack.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VECTOR_E131_DATA_PACKET 0x00000002u

#define VECTOR_E131_EXTENDED_SYNCHRONIZATION 0x00000001u
#define VECTOR_E131_EXTENDED_DISCOVERY 0x00000002u

#define VECTOR_UNIVERSE_DISCOVERY_UNIVERSE_LIST 0x00000001

#define SACN_OPTVAL_PREVIEW 0x80u
#define SACN_OPTVAL_TERMINATED 0x40u
#define SACN_OPTVAL_FORCE_SYNC 0x20u

#define SACN_DATA_HEADER_SIZE 126
#define SACN_SYNC_PDU_SIZE 49
#define SACN_UNIVERSE_DISCOVERY_HEADER_SIZE 120
#define SACN_MAX_UNIVERSES_PER_PAGE 512

#define SACN_PRI_OFFSET 108
#define SACN_SEQ_OFFSET 111
#define SACN_OPTS_OFFSET 112
#define SACN_START_CODE_OFFSET 125

#define SACN_ROOT_VECTOR_OFFSET ACN_UDP_PREAMBLE_SIZE + 2
#define SACN_FRAMING_OFFSET 38
#define SACN_FRAMING_VECTOR_OFFSET 40
#define SACN_SOURCE_NAME_OFFSET 44
#define SACN_DMP_OFFSET 115
#define SACN_PROPERTY_VALUE_COUNT_OFFSET 123
#define SACN_UNIVERSE_DISCOVERY_OFFSET 112
#define SACN_UNIVERSE_DISCOVERY_PAGE_OFFSET SACN_UNIVERSE_DISCOVERY_OFFSET + 6
#define SACN_UNIVERSE_DISCOVERY_LAST_PAGE_OFFSET SACN_UNIVERSE_DISCOVERY_OFFSET + 7

#define SET_SEQUENCE(bufptr, seq) (bufptr[SACN_SEQ_OFFSET] = seq)
#define SET_FORCE_SYNC_OPT(bufptr, force_sync) /* TODO                \
  do                                                                  \
  {                                                                   \
    if (force_sync)                                                   \
      bufptr[SACN_OPTS_OFFSET] |= SACN_OPTVAL_FORCE_SYNC;             \
    else                                                              \
      bufptr[SACN_OPTS_OFFSET] &= ~(uint8_t)(SACN_OPTVAL_FORCE_SYNC); \
  } while (0) */
#define SET_TERMINATED_OPT(bufptr, terminated)                        \
  do                                                                  \
  {                                                                   \
    if (terminated)                                                   \
      bufptr[SACN_OPTS_OFFSET] |= SACN_OPTVAL_TERMINATED;             \
    else                                                              \
      bufptr[SACN_OPTS_OFFSET] &= ~(uint8_t)(SACN_OPTVAL_TERMINATED); \
  } while (0)
#define TERMINATED_OPT_SET(bufptr) (bufptr[SACN_OPTS_OFFSET] & SACN_OPTVAL_TERMINATED)
#define SET_PREVIEW_OPT(bufptr, preview)                           \
  do                                                               \
  {                                                                \
    if (preview)                                                   \
      bufptr[SACN_OPTS_OFFSET] |= SACN_OPTVAL_PREVIEW;             \
    else                                                           \
      bufptr[SACN_OPTS_OFFSET] &= ~(uint8_t)(SACN_OPTVAL_PREVIEW); \
  } while (0)
#define SET_PRIORITY(bufptr, priority) ((void)(bufptr[SACN_PRI_OFFSET] = priority));
#define SET_DATA_SLOT_COUNT(bufptr, slot_count)                                                                      \
  do                                                                                                                 \
  {                                                                                                                  \
    ACN_PDU_PACK_NORMAL_LEN(&bufptr[ACN_UDP_PREAMBLE_SIZE],                                                          \
                            SACN_DATA_HEADER_SIZE + slot_count - ACN_UDP_PREAMBLE_SIZE);                             \
    ACN_PDU_PACK_NORMAL_LEN(&bufptr[SACN_FRAMING_OFFSET], SACN_DATA_HEADER_SIZE + slot_count - SACN_FRAMING_OFFSET); \
    ACN_PDU_PACK_NORMAL_LEN(&bufptr[SACN_DMP_OFFSET], SACN_DATA_HEADER_SIZE + slot_count - SACN_DMP_OFFSET);         \
    etcpal_pack_u16b(&bufptr[SACN_PROPERTY_VALUE_COUNT_OFFSET], 1 + slot_count);                                     \
  } while (0)
#define SET_UNIVERSE_COUNT(bufptr, count)                                                                         \
  do                                                                                                              \
  {                                                                                                               \
    ACN_PDU_PACK_NORMAL_LEN(&bufptr[ACN_UDP_PREAMBLE_SIZE],                                                       \
                            SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (count * 2u) - ACN_UDP_PREAMBLE_SIZE);          \
    ACN_PDU_PACK_NORMAL_LEN(&bufptr[SACN_FRAMING_OFFSET],                                                         \
                            SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (count * 2u) - SACN_FRAMING_OFFSET);            \
    ACN_PDU_PACK_NORMAL_LEN(&bufptr[SACN_UNIVERSE_DISCOVERY_OFFSET],                                              \
                            SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (count * 2u) - SACN_UNIVERSE_DISCOVERY_OFFSET); \
  } while (0)
#define SET_PAGE(bufptr, page) (bufptr[SACN_UNIVERSE_DISCOVERY_PAGE_OFFSET] = page)
#define SET_LAST_PAGE(bufptr, last_page) (bufptr[SACN_UNIVERSE_DISCOVERY_LAST_PAGE_OFFSET] = last_page)

bool parse_sacn_data_packet(const uint8_t* buf, size_t buflen, SacnRemoteSource* source_info, uint8_t* seq,
                            bool* terminated, SacnRecvUniverseData* universe_data);
bool parse_framing_layer_vector(const uint8_t* buf, size_t buflen, uint32_t* vector);
bool parse_sacn_universe_discovery_layer(const uint8_t* buf, size_t buflen, int* page, int* last_page,
                                         const uint8_t** universes, size_t* num_universes);
bool parse_sacn_universe_list(const uint8_t* buf, size_t num_universes, uint16_t* universe_list);
int pack_sacn_root_layer(uint8_t* buf, uint16_t pdu_length, bool extended, const EtcPalUuid* source_cid);
int pack_sacn_data_framing_layer(uint8_t* buf, uint16_t slot_count, uint32_t vector, const char* source_name,
                                 uint8_t priority, uint16_t sync_address, uint8_t seq_num, bool preview,
                                 bool terminated, bool force_sync, uint16_t universe_id);
int pack_sacn_dmp_layer_header(uint8_t* buf, uint8_t start_code, uint16_t slot_count);
int pack_sacn_sync_framing_layer(uint8_t* buf, uint8_t seq_num, uint16_t sync_address);
int pack_sacn_universe_discovery_framing_layer(uint8_t* buf, uint16_t universe_count, const char* source_name);
int pack_sacn_universe_discovery_layer_header(uint8_t* buf, uint16_t universe_count, uint8_t page, uint8_t last_page);
void init_sacn_data_send_buf(uint8_t* send_buf, uint8_t start_code, const EtcPalUuid* source_cid,
                             const char* source_name, uint8_t priority, uint16_t universe, uint16_t sync_universe,
                             bool send_preview);

void update_send_buf_data(uint8_t* send_buf, const uint8_t* new_data, uint16_t new_data_size,
                          force_sync_behavior_t force_sync);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_PDU_H_ */
