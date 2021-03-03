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

#include "sacn/private/receiver_state.h"

#include <limits>
#include <optional>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/thread.h"
#include "etcpal_mock/timer.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/sockets.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/pdu.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestReceiverState TestReceiverStateDynamic
#else
#define TestReceiverState TestReceiverStateStatic
#endif

static const SacnReceiverCallbacks kTestCallbacks = {
    [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*, bool, void*) {},
    [](sacn_receiver_t, uint16_t, const SacnLostSource*, size_t, void*) {},
    NULL,
    NULL,
    NULL,
    NULL};
static const SacnReceiverConfig kTestReceiverConfig = {0u, kTestCallbacks, SACN_RECEIVER_INFINITE_SOURCES, 0u,
                                                       kSacnIpV4AndIpV6};

static std::vector<SacnMcastInterface> kTestNetints = {{{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}};

class TestReceiverState : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_sockets_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_receiver_state_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_receiver_state_deinit();
    sacn_mem_deinit();
  }

  SacnReceiver* AddReceiver(uint16_t universe_id, const SacnReceiverCallbacks& callbacks = kTestCallbacks)
  {
    SacnReceiverConfig config = kTestReceiverConfig;
    config.universe_id = universe_id;
    config.callbacks = callbacks;

    SacnReceiver* receiver = nullptr;
    EXPECT_EQ(add_sacn_receiver(next_receiver_handle_++, &config, kTestNetints.data(), kTestNetints.size(), &receiver),
              kEtcPalErrOk);

    return receiver;
  }

  sacn_receiver_t next_receiver_handle_ = 0;
};

TEST_F(TestReceiverState, ExpiredWaitInitializes)
{
  EXPECT_EQ(get_expired_wait(), SACN_DEFAULT_EXPIRED_WAIT_MS);
}

TEST_F(TestReceiverState, DeinitJoinsInitializedThread)
{
  EXPECT_EQ(etcpal_thread_join_fake.call_count, 0u);

  SacnReceiver* receiver = AddReceiver(1u);
  assign_receiver_to_thread(receiver);

  sacn_receiver_state_deinit();

  EXPECT_EQ(etcpal_thread_join_fake.call_count, 1u);
}
