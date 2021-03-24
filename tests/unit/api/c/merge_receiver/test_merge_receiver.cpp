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

#include "sacn/merge_receiver.h"

#include <limits>
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/dmx_merger.h"
#include "sacn_mock/private/receiver.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/merge_receiver.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestMergeReceiver TestMergeReceiverDynamic
#else
#define TestMergeReceiver TestMergeReceiverStatic
#endif

static constexpr uint16_t kTestUniverse = 123u;
static constexpr int kTestHandle = 4567u;
static constexpr int kTestHandle2 = 1234u;
static constexpr SacnMergeReceiverConfig kTestConfig = {
    kTestUniverse,
    {[](sacn_merge_receiver_t, uint16_t, const uint8_t*, const sacn_source_id_t*, void*) {},
     [](sacn_merge_receiver_t, uint16_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*, void*) {}, NULL,
     NULL},
    SACN_RECEIVER_INFINITE_SOURCES,
    true,
    kSacnIpV4AndIpV6};

class TestMergeReceiver : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_dmx_merger_reset_all_fakes();
    sacn_receiver_reset_all_fakes();

    sacn_receiver_create_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t* handle, SacnMcastInterface*,
                                               size_t) {
      *handle = kTestHandle;
      return kEtcPalErrOk;
    };

    sacn_dmx_merger_create_fake.custom_fake = [](const SacnDmxMergerConfig*, sacn_dmx_merger_t* handle) {
      *handle = kTestHandle;
      return kEtcPalErrOk;
    };

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_merge_receiver_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_merge_receiver_deinit();
    sacn_mem_deinit();
  }
};

TEST_F(TestMergeReceiver, CreateWorks)
{
  SacnMergeReceiverConfig config = kTestConfig;
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&config, &handle, nullptr, 0u), kEtcPalErrOk);

  EXPECT_EQ(handle, kTestHandle);
  EXPECT_EQ(sacn_receiver_create_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_create_fake.call_count, 1u);

  SacnMergeReceiver* merge_receiver = NULL;
  ASSERT_EQ(lookup_merge_receiver(handle, &merge_receiver, NULL), kEtcPalErrOk);
  EXPECT_EQ(merge_receiver->merge_receiver_handle, handle);
  EXPECT_EQ(merge_receiver->merger_handle, kTestHandle);
  EXPECT_TRUE(merge_receiver->use_pap);
  EXPECT_EQ(get_num_merge_receivers(), 1u);

  // Now test failure cleanup
  sacn_receiver_create_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t* handle, SacnMcastInterface*,
                                             size_t) {
    *handle = kTestHandle2;
    return kEtcPalErrOk;
  };

  sacn_dmx_merger_create_fake.custom_fake = [](const SacnDmxMergerConfig*, sacn_dmx_merger_t*) {
    return kEtcPalErrSys;
  };

  ++config.universe_id;
  EXPECT_EQ(sacn_merge_receiver_create(&config, &handle, nullptr, 0u), kEtcPalErrSys);

  EXPECT_EQ(get_num_merge_receivers(), 1u);

  EXPECT_EQ(sacn_receiver_destroy_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_destroy_fake.call_count, 0u);
}

TEST_F(TestMergeReceiver, DestroyWorks)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);
  EXPECT_EQ(get_num_merge_receivers(), 1u);

  EXPECT_EQ(sacn_merge_receiver_destroy(handle), kEtcPalErrOk);
  EXPECT_EQ(get_num_merge_receivers(), 0u);
}

TEST_F(TestMergeReceiver, ChangeUniverseWorks)
{
  static constexpr size_t kNumSources = 5u;

  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  SacnMergeReceiver* merge_receiver = nullptr;
  ASSERT_EQ(lookup_merge_receiver(handle, &merge_receiver, nullptr), kEtcPalErrOk);
  for (size_t i = 0u; i < kNumSources; ++i)
  {
    EXPECT_EQ(
        add_sacn_merge_receiver_source(merge_receiver, static_cast<sacn_source_id_t>(i), &etcpal::Uuid::V4().get()),
        kEtcPalErrOk);
  }

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->cids_from_ids), kNumSources);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->ids_from_cids), kNumSources);

  EXPECT_EQ(sacn_merge_receiver_change_universe(handle, kTestUniverse + 1u), kEtcPalErrOk);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->cids_from_ids), 0u);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->ids_from_cids), 0u);

  EXPECT_EQ(sacn_receiver_change_universe_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_remove_source_fake.call_count, kNumSources);
}

TEST_F(TestMergeReceiver, ResetNetworkingPerReceiverWorks)
{
  static constexpr size_t kNumNetintLists = 7u;

  SacnMergeReceiverNetintList netint_lists[kNumNetintLists];

  for (size_t i = 0u; i < kNumNetintLists; ++i)
  {
    netint_lists[i].handle = (sacn_merge_receiver_t)i;
    netint_lists[i].netints = nullptr;
    netint_lists[i].num_netints = 0u;
  }

  sacn_receiver_reset_networking_per_receiver_fake.custom_fake = [](const SacnReceiverNetintList* netint_lists,
                                                                    size_t num_netint_lists) {
    for (size_t i = 0u; i < kNumNetintLists; ++i)
    {
      EXPECT_EQ(netint_lists[i].handle, (sacn_receiver_t)i);
      EXPECT_EQ(netint_lists[i].netints, nullptr);
      EXPECT_EQ(netint_lists[i].num_netints, 0u);
      EXPECT_EQ(num_netint_lists, kNumNetintLists);
    }

    return kEtcPalErrOk;
  };

  EXPECT_EQ(sacn_merge_receiver_reset_networking_per_receiver(netint_lists, kNumNetintLists), kEtcPalErrOk);
  EXPECT_EQ(sacn_receiver_reset_networking_per_receiver_fake.call_count, 1u);
}
