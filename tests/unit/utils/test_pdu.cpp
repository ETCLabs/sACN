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

  void InitDataPacket(uint8_t* output, const SacnHeaderData& header, uint8_t seq, bool terminated, const uint8_t* pdata)
  {
    memset(output, 0, SACN_MTU);
    uint8_t* pcur = output;

    pcur = InitRootLayer(pcur, header);
    pcur = InitFramingLayer(pcur, header, seq, terminated);
    InitDmpLayer(pcur, header, pdata);
  }

  uint8_t* InitRootLayer(uint8_t* output, const SacnHeaderData& header)
  {
    return InitRootLayer(output, SACN_DATA_HEADER_SIZE + header.slot_count, false, header.cid);
  }

  uint8_t* InitRootLayer(uint8_t* output, uint16_t pdu_length, bool extended, const EtcPalUuid& source_cid)
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

  uint8_t* InitFramingLayer(uint8_t* output, const SacnHeaderData& header, uint8_t seq, bool terminated)
  {
    return InitFramingLayer(output, header.slot_count, VECTOR_E131_DATA_PACKET, header.source_name, header.priority,
                            seq, header.preview, terminated, header.universe_id);
  }

  uint8_t* InitFramingLayer(uint8_t* output, uint16_t slot_count, uint32_t vector, const char* source_name,
                            uint8_t priority, uint8_t seq_num, bool preview, bool terminated, uint16_t universe_id)
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

  uint8_t* InitDmpLayer(uint8_t* output, const SacnHeaderData& header, const uint8_t* pdata)
  {
    return InitDmpLayer(output, header.start_code, header.slot_count, pdata);
  }

  uint8_t* InitDmpLayer(uint8_t* output, uint8_t start_code, uint16_t slot_count, const uint8_t* pdata)
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
    etcpal_pack_u16b(pcur, slot_count + 1u);  // Property value count
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

  void TestParseDataPacket(const SacnHeaderData& header, uint8_t seq, bool terminated,
                           const std::vector<uint8_t>& pdata)
  {
    InitDataPacket(test_buffer_, header, seq, terminated, pdata.data());

    SacnHeaderData header_out;
    uint8_t seq_out;
    bool terminated_out;
    const uint8_t* pdata_out;
    EXPECT_TRUE(parse_sacn_data_packet(&test_buffer_[SACN_FRAMING_OFFSET], SACN_MTU - SACN_FRAMING_OFFSET, &header_out,
                                       &seq_out, &terminated_out, &pdata_out));

    EXPECT_EQ(strcmp(header_out.source_name, header.source_name), 0);
    EXPECT_EQ(header_out.universe_id, header.universe_id);
    EXPECT_EQ(header_out.priority, header.priority);
    EXPECT_EQ(header_out.preview, header.preview);
    EXPECT_EQ(header_out.start_code, header.start_code);
    EXPECT_EQ(header_out.slot_count, header.slot_count);
    EXPECT_EQ(seq_out, seq);
    EXPECT_EQ(terminated_out, terminated);
    EXPECT_EQ(memcmp(pdata_out, pdata.data(), header.slot_count), 0);
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
  SacnHeaderData header;
  header.cid = kEtcPalNullUuid;
  strcpy(header.source_name, "Test Name");
  header.universe_id = 1u;
  header.priority = 100u;
  header.preview = true;
  header.start_code = 0x00;
  header.slot_count = static_cast<uint16_t>(data1.size());

  TestParseDataPacket(header, 1u, false, data1);

  std::vector<uint8_t> data2 = {7u, 6u, 5u, 4u, 3u};
  header.cid = kEtcPalNullUuid;
  strcpy(header.source_name, "Name Test");
  header.universe_id = 123u;
  header.priority = 64;
  header.preview = false;
  header.start_code = 0xDD;
  header.slot_count = static_cast<uint16_t>(data2.size());
  TestParseDataPacket(header, 10u, true, data2);

  std::vector<uint8_t> max_data;
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    max_data.push_back(static_cast<uint8_t>(i));
  header.cid = kEtcPalNullUuid;
  strcpy(header.source_name, "012345678901234567890123456789012345678901234567890123456789012");
  header.universe_id = 0xFFFFu;
  header.priority = 0xFF;
  header.preview = true;
  header.start_code = 0xFF;
  header.slot_count = DMX_ADDRESS_COUNT;
  TestParseDataPacket(header, 0xFFu, true, max_data);
}

TEST_F(TestPdu, ParseSacnDataPacketHandlesInvalid)
{
  static const SacnHeaderData kValidHeader = {kEtcPalNullUuid, 1u, "Test Name", 1u, 100u, true, 0x00, 3u};
  static const std::vector<uint8_t> kValidData = {1u, 2u, 3u};
  static constexpr size_t kBufLenTooShort = 87u;
  static constexpr uint32_t kNonDataVector = (VECTOR_E131_DATA_PACKET + 123u);
  static constexpr uint8_t kInvalidDmpVector = 0x04;
  static constexpr uint8_t kInvalidAddressDataType = 0x12;
  static constexpr uint16_t kInvalidFirstPropertyAddr = 0x9876;
  static constexpr uint16_t kInvalidAddrIncrement = 0x1234;
  static const size_t kValidBufferLength = (SACN_DATA_HEADER_SIZE + kValidData.size() - SACN_FRAMING_OFFSET);

  SacnHeaderData header_out;
  uint8_t seq_out;
  bool terminated_out;
  const uint8_t* pdata_out;

  uint8_t valid_data[SACN_MTU];
  InitDataPacket(valid_data, kValidHeader, 1u, false, kValidData.data());
  EXPECT_TRUE(parse_sacn_data_packet(&valid_data[SACN_FRAMING_OFFSET], kValidBufferLength, &header_out, &seq_out,
                                     &terminated_out, &pdata_out));

  // Start with null pointers and short buffer length
  EXPECT_FALSE(parse_sacn_data_packet(nullptr, kValidBufferLength, &header_out, &seq_out, &terminated_out, &pdata_out));
  EXPECT_FALSE(parse_sacn_data_packet(&valid_data[SACN_FRAMING_OFFSET], kBufLenTooShort, &header_out, &seq_out,
                                      &terminated_out, &pdata_out));
  EXPECT_FALSE(parse_sacn_data_packet(&valid_data[SACN_FRAMING_OFFSET], kValidBufferLength, nullptr, &seq_out,
                                      &terminated_out, &pdata_out));
  EXPECT_FALSE(parse_sacn_data_packet(&valid_data[SACN_FRAMING_OFFSET], kValidBufferLength, &header_out, nullptr,
                                      &terminated_out, &pdata_out));
  EXPECT_FALSE(parse_sacn_data_packet(&valid_data[SACN_FRAMING_OFFSET], kValidBufferLength, &header_out, &seq_out,
                                      nullptr, &pdata_out));
  EXPECT_FALSE(parse_sacn_data_packet(&valid_data[SACN_FRAMING_OFFSET], kValidBufferLength, &header_out, &seq_out,
                                      &terminated_out, nullptr));

  // Now test buffer defects
  uint8_t vector_not_data[SACN_MTU];
  InitDataPacket(vector_not_data, kValidHeader, 1u, false, kValidData.data());
  etcpal_pack_u32b(&vector_not_data[SACN_FRAMING_OFFSET + 2], kNonDataVector);
  EXPECT_FALSE(parse_sacn_data_packet(&vector_not_data[SACN_FRAMING_OFFSET], kValidBufferLength, &header_out, &seq_out,
                                      &terminated_out, &pdata_out));
  uint8_t invalid_dmp_vector[SACN_MTU];
  InitDataPacket(invalid_dmp_vector, kValidHeader, 1u, false, kValidData.data());
  invalid_dmp_vector[SACN_FRAMING_OFFSET + 79] = kInvalidDmpVector;
  EXPECT_FALSE(parse_sacn_data_packet(&invalid_dmp_vector[SACN_FRAMING_OFFSET], kValidBufferLength, &header_out,
                                      &seq_out, &terminated_out, &pdata_out));
  uint8_t invalid_address_data_type[SACN_MTU];
  InitDataPacket(invalid_address_data_type, kValidHeader, 1u, false, kValidData.data());
  invalid_address_data_type[SACN_FRAMING_OFFSET + 80] = kInvalidAddressDataType;
  EXPECT_FALSE(parse_sacn_data_packet(&invalid_address_data_type[SACN_FRAMING_OFFSET], kValidBufferLength, &header_out,
                                      &seq_out, &terminated_out, &pdata_out));
  uint8_t invalid_first_property_addr[SACN_MTU];
  InitDataPacket(invalid_first_property_addr, kValidHeader, 1u, false, kValidData.data());
  etcpal_pack_u16b(&invalid_first_property_addr[SACN_FRAMING_OFFSET + 81], kInvalidFirstPropertyAddr);
  EXPECT_FALSE(parse_sacn_data_packet(&invalid_first_property_addr[SACN_FRAMING_OFFSET], kValidBufferLength,
                                      &header_out, &seq_out, &terminated_out, &pdata_out));
  uint8_t invalid_addr_increment[SACN_MTU];
  InitDataPacket(invalid_addr_increment, kValidHeader, 1u, false, kValidData.data());
  etcpal_pack_u16b(&invalid_addr_increment[SACN_FRAMING_OFFSET + 83], kInvalidAddrIncrement);
  EXPECT_FALSE(parse_sacn_data_packet(&invalid_addr_increment[SACN_FRAMING_OFFSET], kValidBufferLength, &header_out,
                                      &seq_out, &terminated_out, &pdata_out));
  uint8_t data_too_big[SACN_MTU];
  InitDataPacket(data_too_big, kValidHeader, 1u, false, kValidData.data());
  etcpal_pack_u16b(&data_too_big[SACN_FRAMING_OFFSET + 85], static_cast<uint16_t>(kValidData.size() + 2u));
  EXPECT_FALSE(parse_sacn_data_packet(&data_too_big[SACN_FRAMING_OFFSET], kValidBufferLength, &header_out, &seq_out,
                                      &terminated_out, &pdata_out));
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
