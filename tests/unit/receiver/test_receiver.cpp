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

#include "sacn/receiver.h"

#include <limits>
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/receiver.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestReceiver TestReceiverDynamic
#else
#define TestReceiver TestReceiverStatic
#endif

class TestReceiver : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_receiver_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_receiver_deinit();
    sacn_mem_deinit();
  }
};

FAKE_VOID_FUNC(handle_universe_data, sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*,
               void*);

FAKE_VOID_FUNC(handle_sources_lost, sacn_receiver_t, const SacnLostSource*, size_t, void*);

FAKE_VOID_FUNC(handle_source_pcp_lost, sacn_receiver_t, const SacnRemoteSource*, void*);

FAKE_VOID_FUNC(handle_sampling_ended, sacn_receiver_t, void*);

FAKE_VOID_FUNC(handle_source_limit_exceeded, sacn_receiver_t, void*);

TEST_F(TestReceiver, SetStandardVersionWorks)
{
  // Initialization should set it to the default
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionAll);

  sacn_receiver_set_standard_version(kSacnStandardVersionDraft);
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionDraft);
  sacn_receiver_set_standard_version(kSacnStandardVersionPublished);
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionPublished);
  sacn_receiver_set_standard_version(kSacnStandardVersionAll);
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionAll);
  sacn_receiver_set_standard_version(kSacnStandardVersionNone);
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionNone);
}

TEST_F(TestReceiver, SetExpiredWaitWorks)
{
  // Initialization should set it to the default
  EXPECT_EQ(sacn_receiver_get_expired_wait(), SACN_DEFAULT_EXPIRED_WAIT_MS);

  sacn_receiver_set_expired_wait(0);
  EXPECT_EQ(sacn_receiver_get_expired_wait(), 0u);
  sacn_receiver_set_expired_wait(5000);
  EXPECT_EQ(sacn_receiver_get_expired_wait(), 5000u);
  sacn_receiver_set_expired_wait(std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(sacn_receiver_get_expired_wait(), std::numeric_limits<uint32_t>::max());
}

TEST_F(TestReceiver, Placeholder)
{
  SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
  config.callbacks = {handle_universe_data, handle_sources_lost, handle_source_pcp_lost, handle_sampling_ended,
                      handle_source_limit_exceeded};
  config.callback_context = this;
  config.universe_id = 1;  // Start at universe 1.

  sacn_receiver_t handle;
  sacn_receiver_create(&config, &handle);
}
