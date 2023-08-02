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

#include "sacn/private/pdu.h"

#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn_mock/private/common.h"
#include "gtest/gtest.h"
#include "fff.h"

// Suppress strncpy() warning on Windows/MSVC.
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#if SACN_DYNAMIC_MEM
#define TestPdu TestPduDynamic
#else
#define TestPdu TestPduStatic
#endif

class TestPdu : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();

    memset(test_buffer_, 0, SACN_MTU);
  }

  void TearDown() override {}

  void InitDataPacket(uint8_t* output, const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data,
                      uint8_t seq, bool terminated)
  {
    memset(output, 0, SACN_MTU);
    uint8_t* pcur = output;

    pcur = InitRootLayer(pcur, source_info, universe_data);
    pcur = InitFramingLayer(pcur, source_info, universe_data, seq, terminated);
    InitDmpLayer(pcur, universe_data);
  }

  uint8_t* InitRootLayer(uint8_t* output, const SacnRemoteSource& source_info,
                         const SacnRecvUniverseData& universe_data)
  {
    return InitRootLayer(output, SACN_DATA_HEADER_SIZE + universe_data.slot_range.address_count, false,
                         source_info.cid);
  }

  uint8_t* InitRootLayer(uint8_t* output, int pdu_length, bool extended, const EtcPalUuid& source_cid)
  {
    uint8_t* pcur = output;

    pcur += acn_pack_udp_preamble(pcur, ACN_UDP_PREAMBLE_SIZE);  // Preamble & Post-amble Sizes + ACN Packet Identifier
    (*pcur) |= 0x70u;                                            // Flags
    ACN_PDU_PACK_NORMAL_LEN(pcur, pdu_length - ACN_UDP_PREAMBLE_SIZE);  // Length
    pcur += 2;
    etcpal_pack_u32b(pcur, extended ? ACN_VECTOR_ROOT_E131_EXTENDED : ACN_VECTOR_ROOT_E131_DATA);  // Vector
    pcur += 4;
    memcpy(pcur, source_cid.data, ETCPAL_UUID_BYTES);  // CID
    pcur += ETCPAL_UUID_BYTES;

    return pcur;
  }

  uint8_t* InitFramingLayer(uint8_t* output, const SacnRemoteSource& source_info,
                            const SacnRecvUniverseData& universe_data, uint8_t seq, bool terminated)
  {
    return InitFramingLayer(output, universe_data.slot_range.address_count, VECTOR_E131_DATA_PACKET, source_info.name,
                            universe_data.priority, seq, universe_data.preview, terminated, universe_data.universe_id);
  }

  uint8_t* InitFramingLayer(uint8_t* output, int slot_count, uint32_t vector, const char* source_name, uint8_t priority,
                            uint8_t seq_num, bool preview, bool terminated, uint16_t universe_id)
  {
    uint8_t* pcur = output;

    (*pcur) |= 0x70u;                                                                         // Flags
    ACN_PDU_PACK_NORMAL_LEN(pcur, SACN_DATA_HEADER_SIZE + slot_count - SACN_FRAMING_OFFSET);  // Length
    pcur += 2;
    etcpal_pack_u32b(pcur, vector);  // Vector
    pcur += 4;
    strncpy((char*)pcur, source_name, SACN_SOURCE_NAME_MAX_LEN);  // Source Name
    pcur += SACN_SOURCE_NAME_MAX_LEN;
    (*pcur) = priority;  // Priority
    ++pcur;
    etcpal_pack_u16b(pcur, 0u);  // TODO: Synchronization Address
    pcur += 2;
    (*pcur) = seq_num;  // Sequence Number
    ++pcur;

    // Options
    if (preview)
      *pcur |= SACN_OPTVAL_PREVIEW;
    if (terminated)
      *pcur |= SACN_OPTVAL_TERMINATED;
    // TODO: force_sync
    ++pcur;

    etcpal_pack_u16b(pcur, universe_id);  // Universe
    pcur += 2;

    return pcur;
  }

  uint8_t* InitDmpLayer(uint8_t* output, const SacnRecvUniverseData& universe_data)
  {
    return InitDmpLayer(output, universe_data.start_code, universe_data.slot_range.address_count, universe_data.values);
  }

  uint8_t* InitDmpLayer(uint8_t* output, uint8_t start_code, int slot_count, const uint8_t* pdata)
  {
    uint8_t* pcur = output;

    (*pcur) |= 0x70u;                                                                     // Flags
    ACN_PDU_PACK_NORMAL_LEN(pcur, SACN_DATA_HEADER_SIZE + slot_count - SACN_DMP_OFFSET);  // Length
    pcur += 2;
    (*pcur) = 0x02u;  // Vector = VECTOR_DMP_SET_PROPERTY
    ++pcur;
    (*pcur) = 0xA1u;  // Address Type & Data Type
    ++pcur;
    etcpal_pack_u16b(pcur, 0x0000u);  // First Property Address
    pcur += 2;
    etcpal_pack_u16b(pcur, 0x0001u);  // Address Increment
    pcur += 2;
    etcpal_pack_u16b(pcur, static_cast<uint16_t>(slot_count) + 1u);  // Property value count
    pcur += 2;
    (*pcur) = start_code;  // DMX512-A START Code
    ++pcur;

    if (pdata)
    {
      memcpy(pcur, pdata, slot_count);  // Data
      pdata += slot_count;
    }

    return pcur;
  }

  void TestParseDataPacket(const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data, uint8_t seq,
                           bool terminated)
  {
    InitDataPacket(test_buffer_, source_info, universe_data, seq, terminated);

    SacnRemoteSource source_info_out;
    SacnRecvUniverseData universe_data_out;
    uint8_t seq_out;
    bool terminated_out;
    EXPECT_TRUE(parse_sacn_data_packet(&test_buffer_[SACN_FRAMING_OFFSET], SACN_MTU - SACN_FRAMING_OFFSET,
                                       &source_info_out, &seq_out, &terminated_out, &universe_data_out));

    EXPECT_EQ(strcmp(source_info_out.name, source_info.name), 0);
    EXPECT_EQ(universe_data_out.universe_id, universe_data.universe_id);
    EXPECT_EQ(universe_data_out.priority, universe_data.priority);
    EXPECT_EQ(universe_data_out.preview, universe_data.preview);
    EXPECT_EQ(universe_data_out.start_code, universe_data.start_code);
    EXPECT_EQ(universe_data_out.slot_range.address_count, universe_data.slot_range.address_count);
    EXPECT_EQ(seq_out, seq);
    EXPECT_EQ(terminated_out, terminated);
    EXPECT_EQ(memcmp(universe_data_out.values, universe_data.values, universe_data.slot_range.address_count), 0);
  }

  void TestPackRootLayer(uint16_t pdu_length, bool extended, const EtcPalUuid& source_cid)
  {
    uint8_t result[SACN_MTU] = {0};
    uint8_t expected[SACN_MTU] = {0};
    int result_length = pack_sacn_root_layer(result, pdu_length, extended, &source_cid);
    int expected_length = (int)(InitRootLayer(expected, pdu_length, extended, source_cid) - expected);

    EXPECT_EQ(result_length, expected_length);
    EXPECT_EQ(memcmp(result, expected, result_length), 0);
  }

  void TestPackDataFramingLayer(uint16_t slot_count, uint32_t vector, const char* source_name, uint8_t priority,
                                uint16_t sync_address, uint8_t seq_num, bool preview, bool terminated, bool force_sync,
                                uint16_t universe_id)
  {
    uint8_t result[SACN_MTU] = {0};
    uint8_t expected[SACN_MTU] = {0};
    int result_length = pack_sacn_data_framing_layer(result, slot_count, vector, source_name, priority, sync_address,
                                                     seq_num, preview, terminated, force_sync, universe_id);
    int expected_length = (int)(InitFramingLayer(expected, slot_count, vector, source_name, priority, seq_num, preview,
                                                 terminated, universe_id) -
                                expected);

    EXPECT_EQ(result_length, expected_length);
    EXPECT_EQ(memcmp(result, expected, result_length), 0);
  }

  void TestPackDmpLayerHeader(uint8_t start_code, uint16_t slot_count)
  {
    uint8_t result[SACN_MTU] = {0};
    uint8_t expected[SACN_MTU] = {0};
    int result_length = pack_sacn_dmp_layer_header(result, start_code, slot_count);
    int expected_length = (int)(InitDmpLayer(expected, start_code, slot_count, nullptr) - expected);

    EXPECT_EQ(result_length, expected_length);
    EXPECT_EQ(memcmp(result, expected, result_length), 0);
  }

  uint8_t test_buffer_[SACN_MTU];
};

TEST_F(TestPdu, SetSequenceWorks)
{
  static constexpr uint8_t kTestSeqNum = 123u;

  uint8_t old_buf[SACN_MTU];
  memcpy(old_buf, test_buffer_, SACN_MTU);

  SET_SEQUENCE(test_buffer_, kTestSeqNum);
  EXPECT_EQ(test_buffer_[SACN_SEQ_OFFSET], kTestSeqNum);
  SET_SEQUENCE(test_buffer_, 0u);
  EXPECT_EQ(memcmp(test_buffer_, old_buf, SACN_MTU), 0);
}

TEST_F(TestPdu, SetTerminatedOptWorks)
{
  uint8_t old_buf[SACN_MTU];
  memcpy(old_buf, test_buffer_, SACN_MTU);

  SET_TERMINATED_OPT(test_buffer_, true);
  EXPECT_GT(test_buffer_[SACN_OPTS_OFFSET] & SACN_OPTVAL_TERMINATED, 0u);
  SET_TERMINATED_OPT(test_buffer_, false);
  EXPECT_EQ(memcmp(test_buffer_, old_buf, SACN_MTU), 0);
}

TEST_F(TestPdu, TerminatedOptSetWorks)
{
  test_buffer_[SACN_OPTS_OFFSET] |= SACN_OPTVAL_TERMINATED;
  EXPECT_TRUE(TERMINATED_OPT_SET(test_buffer_));
  test_buffer_[SACN_OPTS_OFFSET] = 0u;
  EXPECT_FALSE(TERMINATED_OPT_SET(test_buffer_));
}

TEST_F(TestPdu, SetPreviewOptWorks)
{
  uint8_t old_buf[SACN_MTU];
  memcpy(old_buf, test_buffer_, SACN_MTU);

  SET_PREVIEW_OPT(test_buffer_, true);
  EXPECT_GT(test_buffer_[SACN_OPTS_OFFSET] & SACN_OPTVAL_PREVIEW, 0u);
  SET_PREVIEW_OPT(test_buffer_, false);
  EXPECT_EQ(memcmp(test_buffer_, old_buf, SACN_MTU), 0);
}

TEST_F(TestPdu, SetPriorityWorks)
{
  static constexpr uint8_t kTestPriority = 64u;

  uint8_t old_buf[SACN_MTU];
  memcpy(old_buf, test_buffer_, SACN_MTU);

  SET_PRIORITY(test_buffer_, kTestPriority);
  EXPECT_EQ(test_buffer_[SACN_PRI_OFFSET], kTestPriority);
  SET_PRIORITY(test_buffer_, 0u);
  EXPECT_EQ(memcmp(test_buffer_, old_buf, SACN_MTU), 0);
}

TEST_F(TestPdu, SetDataSlotCountWorks)
{
  uint16_t test_count = 256;

  SET_DATA_SLOT_COUNT(test_buffer_, test_count);
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[ACN_UDP_PREAMBLE_SIZE])),
            static_cast<uint32_t>(SACN_DATA_HEADER_SIZE + test_count - ACN_UDP_PREAMBLE_SIZE));
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[SACN_FRAMING_OFFSET])),
            static_cast<uint32_t>(SACN_DATA_HEADER_SIZE + test_count - SACN_FRAMING_OFFSET));
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[SACN_DMP_OFFSET])),
            static_cast<uint32_t>(SACN_DATA_HEADER_SIZE + test_count - SACN_DMP_OFFSET));

  SET_DATA_SLOT_COUNT(test_buffer_, 0u);
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[ACN_UDP_PREAMBLE_SIZE])),
            static_cast<uint32_t>(SACN_DATA_HEADER_SIZE - ACN_UDP_PREAMBLE_SIZE));
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[SACN_FRAMING_OFFSET])),
            static_cast<uint32_t>(SACN_DATA_HEADER_SIZE - SACN_FRAMING_OFFSET));
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[SACN_DMP_OFFSET])),
            static_cast<uint32_t>(SACN_DATA_HEADER_SIZE - SACN_DMP_OFFSET));
}

TEST_F(TestPdu, SetUniverseCountWorks)
{
  uint16_t test_count = 256;

  SET_UNIVERSE_COUNT(test_buffer_, test_count);
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[ACN_UDP_PREAMBLE_SIZE])),
            static_cast<uint32_t>(SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (test_count * 2u) - ACN_UDP_PREAMBLE_SIZE));
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[SACN_FRAMING_OFFSET])),
            static_cast<uint32_t>(SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (test_count * 2u) - SACN_FRAMING_OFFSET));
  EXPECT_EQ(
      ACN_PDU_LENGTH((&test_buffer_[SACN_UNIVERSE_DISCOVERY_OFFSET])),
      static_cast<uint32_t>(SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (test_count * 2u) - SACN_UNIVERSE_DISCOVERY_OFFSET));

  SET_UNIVERSE_COUNT(test_buffer_, 0u);
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[ACN_UDP_PREAMBLE_SIZE])),
            static_cast<uint32_t>(SACN_UNIVERSE_DISCOVERY_HEADER_SIZE - ACN_UDP_PREAMBLE_SIZE));
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[SACN_FRAMING_OFFSET])),
            static_cast<uint32_t>(SACN_UNIVERSE_DISCOVERY_HEADER_SIZE - SACN_FRAMING_OFFSET));
  EXPECT_EQ(ACN_PDU_LENGTH((&test_buffer_[SACN_UNIVERSE_DISCOVERY_OFFSET])),
            static_cast<uint32_t>(SACN_UNIVERSE_DISCOVERY_HEADER_SIZE - SACN_UNIVERSE_DISCOVERY_OFFSET));
}

TEST_F(TestPdu, SetPageWorks)
{
  static constexpr uint8_t kTestPage = 12u;

  uint8_t old_buf[SACN_MTU];
  memcpy(old_buf, test_buffer_, SACN_MTU);

  SET_PAGE(test_buffer_, kTestPage);
  EXPECT_EQ(test_buffer_[SACN_UNIVERSE_DISCOVERY_PAGE_OFFSET], kTestPage);
  SET_PAGE(test_buffer_, 0u);
  EXPECT_EQ(memcmp(test_buffer_, old_buf, SACN_MTU), 0);
}

TEST_F(TestPdu, SetLastPageWorks)
{
  static constexpr uint8_t kTestPage = 12u;

  uint8_t old_buf[SACN_MTU];
  memcpy(old_buf, test_buffer_, SACN_MTU);

  SET_LAST_PAGE(test_buffer_, kTestPage);
  EXPECT_EQ(test_buffer_[SACN_UNIVERSE_DISCOVERY_LAST_PAGE_OFFSET], kTestPage);
  SET_LAST_PAGE(test_buffer_, 0u);
  EXPECT_EQ(memcmp(test_buffer_, old_buf, SACN_MTU), 0);
}

TEST_F(TestPdu, ParseSacnDataPacketWorks)
{
  std::vector<uint8_t> data1 = {1u, 2u, 3u};
  SacnRemoteSource source_info;
  SacnRecvUniverseData universe_data;
  source_info.cid = kEtcPalNullUuid;
  strcpy(source_info.name, "Test Name");
  universe_data.universe_id = 1u;
  universe_data.priority = 100u;
  universe_data.preview = true;
  universe_data.start_code = SACN_STARTCODE_DMX;
  universe_data.slot_range.address_count = static_cast<uint16_t>(data1.size());
  universe_data.values = data1.data();

  TestParseDataPacket(source_info, universe_data, 1u, false);

  std::vector<uint8_t> data2 = {7u, 6u, 5u, 4u, 3u};
  source_info.cid = kEtcPalNullUuid;
  strcpy(source_info.name, "Name Test");
  universe_data.universe_id = 123u;
  universe_data.priority = 64;
  universe_data.preview = false;
  universe_data.start_code = SACN_STARTCODE_PRIORITY;
  universe_data.slot_range.address_count = static_cast<uint16_t>(data2.size());
  universe_data.values = data2.data();
  TestParseDataPacket(source_info, universe_data, 10u, true);

  std::vector<uint8_t> max_data;
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    max_data.push_back(static_cast<uint8_t>(i));
  source_info.cid = kEtcPalNullUuid;
  strcpy(source_info.name, "012345678901234567890123456789012345678901234567890123456789012");
  universe_data.universe_id = 0xFFFFu;
  universe_data.priority = 0xFF;
  universe_data.preview = true;
  universe_data.start_code = 0xFF;
  universe_data.slot_range.address_count = DMX_ADDRESS_COUNT;
  universe_data.values = max_data.data();
  TestParseDataPacket(source_info, universe_data, 0xFFu, true);
}

TEST_F(TestPdu, ParseSacnDataPacketHandlesInvalid)
{
  static const std::vector<uint8_t> kValidData = {1u, 2u, 3u};
  static const SacnRemoteSource kValidSourceInfo = {1u, kEtcPalNullUuid, "Test Name"};
  static const SacnRecvUniverseData kValidUniverseData = {
      1u, 100u, true, false, SACN_STARTCODE_DMX, {1, 3}, kValidData.data()};
  static constexpr size_t kBufLenTooShort = 87u;
  static constexpr uint32_t kNonDataVector = (VECTOR_E131_DATA_PACKET + 123u);
  static constexpr uint8_t kInvalidDmpVector = 0x04;
  static constexpr uint8_t kInvalidAddressDataType = 0x12;
  static constexpr uint16_t kInvalidFirstPropertyAddr = 0x9876;
  static constexpr uint16_t kInvalidAddrIncrement = 0x1234;
  static const size_t kValidBufferLength = (SACN_DATA_HEADER_SIZE + kValidData.size() - SACN_FRAMING_OFFSET);

  SacnRemoteSource source_info_out;
  SacnRecvUniverseData universe_data_out;
  uint8_t seq_out;
  bool terminated_out;

  uint8_t valid_data[SACN_MTU];
  InitDataPacket(valid_data, kValidSourceInfo, kValidUniverseData, 1u, false);
  EXPECT_TRUE(parse_sacn_data_packet(&valid_data[SACN_FRAMING_OFFSET], kValidBufferLength, &source_info_out, &seq_out,
                                     &terminated_out, &universe_data_out));

  // Start with short buffer length
  EXPECT_FALSE(parse_sacn_data_packet(&valid_data[SACN_FRAMING_OFFSET], kBufLenTooShort, &source_info_out, &seq_out,
                                      &terminated_out, &universe_data_out));

  // Now test buffer defects
  uint8_t vector_not_data[SACN_MTU];
  InitDataPacket(vector_not_data, kValidSourceInfo, kValidUniverseData, 1u, false);
  etcpal_pack_u32b(&vector_not_data[SACN_FRAMING_OFFSET + 2], kNonDataVector);
  EXPECT_FALSE(parse_sacn_data_packet(&vector_not_data[SACN_FRAMING_OFFSET], kValidBufferLength, &source_info_out,
                                      &seq_out, &terminated_out, &universe_data_out));
  uint8_t invalid_dmp_vector[SACN_MTU];
  InitDataPacket(invalid_dmp_vector, kValidSourceInfo, kValidUniverseData, 1u, false);
  invalid_dmp_vector[SACN_FRAMING_OFFSET + 79] = kInvalidDmpVector;
  EXPECT_FALSE(parse_sacn_data_packet(&invalid_dmp_vector[SACN_FRAMING_OFFSET], kValidBufferLength, &source_info_out,
                                      &seq_out, &terminated_out, &universe_data_out));
  uint8_t invalid_address_data_type[SACN_MTU];
  InitDataPacket(invalid_address_data_type, kValidSourceInfo, kValidUniverseData, 1u, false);
  invalid_address_data_type[SACN_FRAMING_OFFSET + 80] = kInvalidAddressDataType;
  EXPECT_FALSE(parse_sacn_data_packet(&invalid_address_data_type[SACN_FRAMING_OFFSET], kValidBufferLength,
                                      &source_info_out, &seq_out, &terminated_out, &universe_data_out));
  uint8_t invalid_first_property_addr[SACN_MTU];
  InitDataPacket(invalid_first_property_addr, kValidSourceInfo, kValidUniverseData, 1u, false);
  etcpal_pack_u16b(&invalid_first_property_addr[SACN_FRAMING_OFFSET + 81], kInvalidFirstPropertyAddr);
  EXPECT_FALSE(parse_sacn_data_packet(&invalid_first_property_addr[SACN_FRAMING_OFFSET], kValidBufferLength,
                                      &source_info_out, &seq_out, &terminated_out, &universe_data_out));
  uint8_t invalid_addr_increment[SACN_MTU];
  InitDataPacket(invalid_addr_increment, kValidSourceInfo, kValidUniverseData, 1u, false);
  etcpal_pack_u16b(&invalid_addr_increment[SACN_FRAMING_OFFSET + 83], kInvalidAddrIncrement);
  EXPECT_FALSE(parse_sacn_data_packet(&invalid_addr_increment[SACN_FRAMING_OFFSET], kValidBufferLength,
                                      &source_info_out, &seq_out, &terminated_out, &universe_data_out));
  uint8_t data_too_big[SACN_MTU];
  InitDataPacket(data_too_big, kValidSourceInfo, kValidUniverseData, 1u, false);
  etcpal_pack_u16b(&data_too_big[SACN_FRAMING_OFFSET + 85], static_cast<uint16_t>(kValidData.size() + 2u));
  EXPECT_FALSE(parse_sacn_data_packet(&data_too_big[SACN_FRAMING_OFFSET], kValidBufferLength, &source_info_out,
                                      &seq_out, &terminated_out, &universe_data_out));
}

TEST_F(TestPdu, PackSacnRootLayerWorks)
{
  TestPackRootLayer(1234u, false, etcpal::Uuid::V4().get());
  TestPackRootLayer(9876u, true, etcpal::Uuid::V4().get());
  TestPackRootLayer(0xFFFFu, true, etcpal::Uuid().get());
}

TEST_F(TestPdu, PackSacnDataFramingLayerWorks)
{
  TestPackDataFramingLayer(0x1234, 0x56789ABC, "A Test Name", 0xDE, 0xF012, 0x34, false, true, false, 0x5678);
  TestPackDataFramingLayer(0xFEDC, 0xBA987654, "Another Test Name", 0x32, 0x10FE, 0xDC, true, false, true, 0xBA98);
  TestPackDataFramingLayer(0xFFFF, 0xFFFFFFFF, "012345678901234567890123456789012345678901234567890123456789012", 0xFF,
                           0xFFFF, 0xFF, true, true, true, 0xFFFF);
}

TEST_F(TestPdu, PackSacnDmpLayerHeaderWorks)
{
  TestPackDmpLayerHeader(0x12, 0x3456);
  TestPackDmpLayerHeader(0xFE, 0xDCBA);
  TestPackDmpLayerHeader(0xFF, 0xFFFF);
}
