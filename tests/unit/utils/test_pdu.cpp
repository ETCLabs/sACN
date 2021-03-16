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

#include "etcpal_mock/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
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

    memset(test_buffer_, 0, SACN_MTU);
  }

  void TearDown() override {}

  void InitTestBuffer(const SacnHeaderData& header, uint8_t seq, bool terminated, const uint8_t* pdata)
  {
    size_t packet_length = SACN_DATA_HEADER_SIZE + header.slot_count;

    uint8_t* pcur = test_buffer_;

    // Root Layer
    pcur += acn_pack_udp_preamble(pcur, ACN_UDP_PREAMBLE_SIZE);  // Preamble & Post-amble Sizes + ACN Packet Identifier
    (*pcur) &= 0x70u;                                            // Flags
    ACN_PDU_PACK_NORMAL_LEN(pcur, packet_length - ACN_UDP_PREAMBLE_SIZE);  // Length
    pcur += 2;
    etcpal_pack_u32b(pcur, ACN_VECTOR_ROOT_E131_DATA);  // Vector
    pcur += 4;
    memcpy(pcur, header.cid.data, ETCPAL_UUID_BYTES);  // CID
    pcur += ETCPAL_UUID_BYTES;

    // E1.31 Framing Layer
    (*pcur) &= 0x70u;                                                    // Flags
    ACN_PDU_PACK_NORMAL_LEN(pcur, packet_length - SACN_FRAMING_OFFSET);  // Length
    pcur += 2;
    etcpal_pack_u32b(pcur, VECTOR_E131_DATA_PACKET);  // Vector
    pcur += 4;
    strncpy((char*)pcur, header.source_name, SACN_SOURCE_NAME_MAX_LEN);  // Source Name
    pcur += SACN_SOURCE_NAME_MAX_LEN;
    (*pcur) = header.priority;  // Priority
    ++pcur;
    etcpal_pack_u16b(pcur, 0u);  // Synchronization Address
    pcur += 2;
    (*pcur) = seq;  // Sequence Number
    ++pcur;

    // Options
    if (header.preview)
      *pcur |= SACN_OPTVAL_PREVIEW;
    if (terminated)
      *pcur |= SACN_OPTVAL_TERMINATED;
    ++pcur;

    etcpal_pack_u16b(pcur, header.universe_id);  // Universe
    pcur += 2;

    // DMP Layer
    (*pcur) &= 0x70u;                                                // Flags
    ACN_PDU_PACK_NORMAL_LEN(pcur, packet_length - SACN_DMP_OFFSET);  // Length
    pcur += 2;
    (*pcur) = 0x02u;  // Vector = VECTOR_DMP_SET_PROPERTY
    ++pcur;
    (*pcur) = 0xA1u;  // Address Type & Data Type
    ++pcur;
    etcpal_pack_u16b(pcur, 0x0000u);  // First Property Address
    pcur += 2;
    etcpal_pack_u16b(pcur, 0x0001u);  // Address Increment
    pcur += 2;
    etcpal_pack_u16b(pcur, header.slot_count + 1u);  // Property value count
    pcur += 2;
    (*pcur) = header.start_code;  // DMX512-A START Code
    ++pcur;
    memcpy(pcur, pdata, header.slot_count);  // Data
    pcur += header.slot_count;

    memset(pcur, 0, SACN_MTU - (pcur - test_buffer_));
  }

  void TestParseDataPacket(const SacnHeaderData& header, uint8_t seq, bool terminated, const uint8_t* pdata)
  {
    InitTestBuffer(header, seq, terminated, pdata);

    SacnHeaderData header_out;
    uint8_t seq_out;
    bool terminated_out;
    const uint8_t *pdata_out;
    EXPECT_TRUE(parse_sacn_data_packet(test_buffer_, SACN_MTU, &header_out, &seq_out, &terminated_out, &pdata_out));

    EXPECT_EQ(ETCPAL_UUID_CMP(&header_out.cid, &header.cid), 0);
    EXPECT_EQ(strcmp(header_out.source_name, header.source_name), 0);
    EXPECT_EQ(header_out.universe_id, header.universe_id);
    EXPECT_EQ(header_out.priority, header.priority);
    EXPECT_EQ(header_out.preview, header.preview);
    EXPECT_EQ(header_out.start_code, header.start_code);
    EXPECT_EQ(header_out.slot_count, header.slot_count);
    EXPECT_EQ(seq_out, seq);
    EXPECT_EQ(terminated_out, terminated);
    EXPECT_EQ(memcmp(pdata_out, pdata, header.slot_count), 0);
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
