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
