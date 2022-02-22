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

#include "sacn/private/common.h"
#include "sacn/private/pdu.h"

#include <string.h>
#include "etcpal/pack.h"

// Suppress strncpy() warning on Windows/MSVC.
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#define SACN_DATA_PACKET_MIN_SIZE 88
#define SACN_DMPVECT_SET_PROPERTY 0x02

bool parse_sacn_data_packet(const uint8_t* buf, size_t buflen, SacnRemoteSource* source_info, uint8_t* seq,
                            bool* terminated, SacnRecvUniverseData* universe_data)
{
  // Check the input parameters including buffer size
  if (!buf || !source_info || !seq || !terminated || !universe_data || buflen < SACN_DATA_PACKET_MIN_SIZE)
    return false;

  // Check the framing layer vector
  if (etcpal_unpack_u32b(&buf[2]) != VECTOR_E131_DATA_PACKET)
    return false;

  // Check the DMP vector and fixed values
  if (buf[79] != SACN_DMPVECT_SET_PROPERTY || buf[80] != 0xa1u || etcpal_unpack_u16b(&buf[81]) != 0x0000u ||
      etcpal_unpack_u16b(&buf[83]) != 0x0001u)
  {
    return false;
  }

  // Make sure the length of the slot data as communicated by the slot count doesn't overflow the
  // data buffer. Slot count value on the wire includes the start code, so subtract 1.
  // TODO: Re-evaluate where this is initialized after footprint implemented
  universe_data->slot_range.start_address = 1;
  universe_data->slot_range.address_count = etcpal_unpack_u16b(&buf[85]) - 1;
  universe_data->values = &buf[88];
  if (universe_data->values + universe_data->slot_range.address_count > buf + buflen)
    return false;

  strncpy(source_info->name, (char*)&buf[6], SACN_SOURCE_NAME_MAX_LEN);
  // Just in case the string is not null terminated even though it is required to be
  source_info->name[SACN_SOURCE_NAME_MAX_LEN - 1] = '\0';
  universe_data->priority = buf[70];
  // TODO universe_data->sync_address = etcpal_unpack_u16b(&buf[71]);
  *seq = buf[73];
  universe_data->preview = (bool)(buf[74] & SACN_OPTVAL_PREVIEW);
  *terminated = (bool)(buf[74] & SACN_OPTVAL_TERMINATED);
  universe_data->universe_id = etcpal_unpack_u16b(&buf[75]);
  universe_data->start_code = buf[87];
  return true;
}

bool parse_framing_layer_vector(const uint8_t* buf, size_t buflen, uint32_t* vector)
{
  // Check the input parameters including buffer size
  if (!buf || !vector || (buflen < 6))
    return false;

  *vector = etcpal_unpack_u32b(&buf[2]);
  return true;
}

bool parse_sacn_universe_discovery_layer(const uint8_t* buf, size_t buflen, int* page, int* last_page,
                                         const uint8_t** universes, size_t* num_universes)
{
  // Check the input parameters including buffer size
  if (!buf || !page || !last_page || !universes || !num_universes || buflen < SACN_DATA_PACKET_MIN_SIZE)
    return false;

  // Check PDU length
  int pdu_length = ACN_PDU_LENGTH(buf);
  if (pdu_length < 8)
    return false;

  // Check the vector
  if (etcpal_unpack_u32b(&buf[2]) != VECTOR_UNIVERSE_DISCOVERY_UNIVERSE_LIST)
    return false;

  *page = buf[6];
  *last_page = buf[7];
  *universes = &buf[8];
  *num_universes = (pdu_length - 8) / 2;

  return true;
}

bool parse_sacn_universe_list(const uint8_t* buf, size_t num_universes, uint16_t* universe_list)
{
  if (!buf || !universe_list)
    return false;

  for (size_t i = 0; i < num_universes; ++i)
    universe_list[i] = etcpal_unpack_u16b(&buf[i * 2]);

  return true;
}

int pack_sacn_root_layer(uint8_t* buf, uint16_t pdu_length, bool extended, const EtcPalUuid* source_cid)
{
  uint8_t* pcur = buf;

  // UDP preamble
  pcur += acn_pack_udp_preamble(pcur, ACN_UDP_PREAMBLE_SIZE);

  // Root layer flags and length
  *pcur = 0;
  ACN_PDU_SET_V_FLAG(*pcur);
  ACN_PDU_SET_H_FLAG(*pcur);
  ACN_PDU_SET_D_FLAG(*pcur);
  ACN_PDU_PACK_NORMAL_LEN(pcur, pdu_length - ACN_UDP_PREAMBLE_SIZE);
  pcur += 2;

  // RLP vector and header
  etcpal_pack_u32b(pcur, extended ? ACN_VECTOR_ROOT_E131_EXTENDED : ACN_VECTOR_ROOT_E131_DATA);
  pcur += 4;
  memcpy(pcur, source_cid->data, ETCPAL_UUID_BYTES);
  pcur += ETCPAL_UUID_BYTES;

  return (int)(pcur - buf);
}

int pack_sacn_data_framing_layer(uint8_t* buf, uint16_t slot_count, uint32_t vector, const char* source_name,
                                 uint8_t priority, uint16_t sync_address, uint8_t seq_num, bool preview,
                                 bool terminated, bool force_sync, uint16_t universe_id)
{
  ETCPAL_UNUSED_ARG(sync_address);  // TODO sacn_sync
  ETCPAL_UNUSED_ARG(force_sync);    // TODO sacn_sync

  uint8_t* pcur = buf;

  // Framing layer flags and length
  *pcur = 0;
  ACN_PDU_SET_V_FLAG(*pcur);
  ACN_PDU_SET_H_FLAG(*pcur);
  ACN_PDU_SET_D_FLAG(*pcur);
  ACN_PDU_PACK_NORMAL_LEN(pcur, SACN_DATA_HEADER_SIZE - SACN_FRAMING_OFFSET + slot_count);
  pcur += 2;

  // Framing layer
  etcpal_pack_u32b(pcur, vector);
  pcur += 4;
  strncpy((char*)pcur, source_name, SACN_SOURCE_NAME_MAX_LEN);
  pcur[SACN_SOURCE_NAME_MAX_LEN - 1] = '\0';
  pcur += SACN_SOURCE_NAME_MAX_LEN;
  *pcur = priority;
  ++pcur;
  etcpal_pack_u16b(pcur, 0 /* TODO sync_address */);
  pcur += 2;
  *pcur = seq_num;
  ++pcur;
  *pcur = 0;
  if (preview)
    *pcur |= SACN_OPTVAL_PREVIEW;
  if (terminated)
    *pcur |= SACN_OPTVAL_TERMINATED;
  // TODO if (force_sync)
  // TODO  *pcur |= SACN_OPTVAL_FORCE_SYNC;
  ++pcur;
  etcpal_pack_u16b(pcur, universe_id);
  pcur += 2;

  return (int)(pcur - buf);
}

int pack_sacn_dmp_layer_header(uint8_t* buf, uint8_t start_code, uint16_t slot_count)
{
  uint8_t* pcur = buf;

  // DMP layer flags and length
  *pcur = 0;
  ACN_PDU_SET_V_FLAG(*pcur);
  ACN_PDU_SET_H_FLAG(*pcur);
  ACN_PDU_SET_D_FLAG(*pcur);
  ACN_PDU_PACK_NORMAL_LEN(pcur, SACN_DATA_HEADER_SIZE - SACN_DMP_OFFSET + slot_count);
  pcur += 2;

  // DMP layer
  *pcur = SACN_DMPVECT_SET_PROPERTY;
  ++pcur;
  *pcur = 0xa1u;
  ++pcur;
  etcpal_pack_u16b(pcur, 0u);
  pcur += 2;
  etcpal_pack_u16b(pcur, 1u);
  pcur += 2;
  etcpal_pack_u16b(pcur, slot_count + 1);
  pcur += 2;
  *pcur = start_code;
  ++pcur;

  return (int)(pcur - buf);
}

int pack_sacn_sync_framing_layer(uint8_t* buf, uint8_t seq_num, uint16_t sync_address)
{
  uint8_t* pcur = buf;

  // Framing layer flags and length
  *pcur = 0;
  ACN_PDU_SET_V_FLAG(*pcur);
  ACN_PDU_SET_H_FLAG(*pcur);
  ACN_PDU_SET_D_FLAG(*pcur);
  ACN_PDU_PACK_NORMAL_LEN(pcur, SACN_SYNC_PDU_SIZE - SACN_FRAMING_OFFSET);
  pcur += 2;

  // Framing layer
  etcpal_pack_u32b(pcur, VECTOR_E131_EXTENDED_SYNCHRONIZATION);
  pcur += 4;
  *pcur = seq_num;
  ++pcur;
  etcpal_pack_u16b(pcur, sync_address);
  pcur += 2;
  etcpal_pack_u16b(pcur, 0u);  // Reserved
  pcur += 2;

  return (int)(pcur - buf);
}

int pack_sacn_universe_discovery_framing_layer(uint8_t* buf, uint16_t universe_count, const char* source_name)
{
  uint8_t* pcur = buf;

  // Framing layer flags and length
  *pcur = 0;
  ACN_PDU_SET_V_FLAG(*pcur);
  ACN_PDU_SET_H_FLAG(*pcur);
  ACN_PDU_SET_D_FLAG(*pcur);
  ACN_PDU_PACK_NORMAL_LEN(pcur, SACN_UNIVERSE_DISCOVERY_HEADER_SIZE - SACN_FRAMING_OFFSET + (universe_count * 2u));
  pcur += 2;

  // Framing layer
  etcpal_pack_u32b(pcur, VECTOR_E131_EXTENDED_DISCOVERY);
  pcur += 4;
  strncpy((char*)pcur, source_name, SACN_SOURCE_NAME_MAX_LEN);
  pcur[SACN_SOURCE_NAME_MAX_LEN - 1] = '\0';
  pcur += SACN_SOURCE_NAME_MAX_LEN;
  etcpal_pack_u32b(pcur, 0u);  // Reserved
  pcur += 4;

  return (int)(pcur - buf);
}

int pack_sacn_universe_discovery_layer_header(uint8_t* buf, uint16_t universe_count, uint8_t page, uint8_t last_page)
{
  uint8_t* pcur = buf;

  // Universe discovery layer flags and length
  *pcur = 0;
  ACN_PDU_SET_V_FLAG(*pcur);
  ACN_PDU_SET_H_FLAG(*pcur);
  ACN_PDU_SET_D_FLAG(*pcur);
  ACN_PDU_PACK_NORMAL_LEN(pcur,
                          SACN_UNIVERSE_DISCOVERY_HEADER_SIZE - SACN_UNIVERSE_DISCOVERY_OFFSET + (universe_count * 2u));
  pcur += 2;

  // Universe discovery layer
  etcpal_pack_u32b(pcur, VECTOR_UNIVERSE_DISCOVERY_UNIVERSE_LIST);
  pcur += 4;
  *pcur = page;
  ++pcur;
  *pcur = last_page;
  ++pcur;

  return (int)(pcur - buf);
}

void init_sacn_data_send_buf(uint8_t* send_buf, uint8_t start_code, const EtcPalUuid* source_cid,
                             const char* source_name, uint8_t priority, uint16_t universe, uint16_t sync_universe,
                             bool send_preview)

{
  memset(send_buf, 0, SACN_DATA_PACKET_MTU);
  int written = 0;
  written += pack_sacn_root_layer(send_buf, SACN_DATA_HEADER_SIZE, false, source_cid);
  written += pack_sacn_data_framing_layer(&send_buf[written], 0, VECTOR_E131_DATA_PACKET, source_name, priority,
                                          sync_universe, 0, send_preview, false, false, universe);
  written += pack_sacn_dmp_layer_header(&send_buf[written], start_code, 0);
}

void update_send_buf_data(uint8_t* send_buf, const uint8_t* new_data, uint16_t new_data_size,
                          force_sync_behavior_t force_sync)
{
  ETCPAL_UNUSED_ARG(force_sync);  // TODO sacn_sync

  // Set force sync flag
  SET_FORCE_SYNC_OPT(send_buf, (force_sync == kEnableForceSync));

  // Update the size/count fields for the new data size (slot count)
  SET_DATA_SLOT_COUNT(send_buf, new_data_size);

  // Copy data into the send buffer immediately after the start code
  memcpy(&send_buf[SACN_DATA_HEADER_SIZE], new_data, new_data_size);
}
