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
#include "etcpal/cpp/inet.h"
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

FAKE_VOID_FUNC(universe_data, sacn_merge_receiver_t, uint16_t, const uint8_t*, const sacn_remote_source_t*,
               void*);
FAKE_VOID_FUNC(universe_non_dmx, sacn_merge_receiver_t, uint16_t, const EtcPalSockAddr*, const SacnHeaderData*,
               const uint8_t*, void*);
FAKE_VOID_FUNC(source_limit_exceeded, sacn_merge_receiver_t, uint16_t, void*);

static constexpr uint16_t kTestUniverse = 123u;
static constexpr uint8_t kTestPriority = 100u;
static constexpr int kTestHandle = 4567u;
static constexpr int kTestHandle2 = 1234u;
static constexpr SacnMergeReceiverConfig kTestConfig = {kTestUniverse,
                                                        {universe_data, universe_non_dmx, source_limit_exceeded, NULL},
                                                        SACN_RECEIVER_INFINITE_SOURCES,
                                                        true,
                                                        kSacnIpV4AndIpV6};
static const EtcPalSockAddr kTestSourceAddr = {SACN_PORT, etcpal::IpAddr::FromString("10.101.1.1").get()};
static const SacnHeaderData kTestHeaderData = {
    etcpal::Uuid::V4().get(), 0u, {'\0'}, kTestUniverse, kTestPriority, false, 0x00, DMX_ADDRESS_COUNT};

class TestMergeReceiver : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_dmx_merger_reset_all_fakes();
    sacn_receiver_reset_all_fakes();

    RESET_FAKE(universe_data);
    RESET_FAKE(universe_non_dmx);
    RESET_FAKE(source_limit_exceeded);

    create_sacn_receiver_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t* handle, SacnMcastInterface*,
                                               size_t) {
      *handle = kTestHandle;
      return kEtcPalErrOk;
    };

    create_sacn_dmx_merger_fake.custom_fake = [](const SacnDmxMergerConfig*, sacn_dmx_merger_t* handle) {
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

  void RunUniverseData(sacn_remote_source_t source_handle, const etcpal::Uuid& source_cid, uint8_t start_code,
                       const std::vector<uint8_t>& pdata, uint8_t priority = kTestPriority)
  {
    SacnHeaderData header = kTestHeaderData;
    header.cid = source_cid.get();
    header.source_handle = source_handle;
    header.priority = priority;
    header.start_code = start_code;
    header.slot_count = static_cast<uint16_t>(pdata.size());
    merge_receiver_universe_data(kTestHandle, &kTestSourceAddr, &header, pdata.data(), false, nullptr);
  }

  void RunSamplingStarted() { merge_receiver_sampling_started(kTestHandle, kTestUniverse, nullptr); }
  void RunSamplingEnded() { merge_receiver_sampling_ended(kTestHandle, kTestUniverse, nullptr); }

  void RunSourcesLost(const std::vector<std::pair<sacn_remote_source_t, etcpal::Uuid>>& handles_cids)
  {
    std::vector<SacnLostSource> lost_sources;
    lost_sources.reserve(handles_cids.size());
    std::transform(handles_cids.begin(), handles_cids.end(), std::back_inserter(lost_sources),
                   [](const std::pair<sacn_remote_source_t, etcpal::Uuid>& handle_cid) {
                     // clang-format off
        SacnLostSource lost_source = {
          handle_cid.first,
          handle_cid.second.get(),
          {'\0'},
          true
        };
                     // clang-format on

                     return lost_source;
                   });

    merge_receiver_sources_lost(kTestHandle, kTestUniverse, lost_sources.data(), lost_sources.size(), nullptr);
  }

  void RunPapLost(sacn_remote_source_t handle, const etcpal::Uuid& cid)
  {
    SacnRemoteSource source = {handle, cid.get(), {'\0'}};
    merge_receiver_pap_lost(kTestHandle, kTestUniverse, &source, nullptr);
  }

  void RunSourceLimitExceeded() { merge_receiver_source_limit_exceeded(kTestHandle, kTestUniverse, nullptr); }
};

TEST_F(TestMergeReceiver, CreateWorks)
{
  SacnMergeReceiverConfig config = kTestConfig;
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&config, &handle, nullptr, 0u), kEtcPalErrOk);

  EXPECT_EQ(handle, kTestHandle);
  EXPECT_EQ(create_sacn_receiver_fake.call_count, 1u);
  EXPECT_EQ(create_sacn_dmx_merger_fake.call_count, 1u);

  SacnMergeReceiver* merge_receiver = NULL;
  ASSERT_EQ(lookup_merge_receiver(handle, &merge_receiver, NULL), kEtcPalErrOk);
  EXPECT_EQ(merge_receiver->merge_receiver_handle, handle);
  EXPECT_EQ(merge_receiver->merger_handle, kTestHandle);
  EXPECT_TRUE(merge_receiver->use_pap);
  EXPECT_EQ(get_num_merge_receivers(), 1u);

  // Now test failure cleanup
  create_sacn_receiver_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t* handle, SacnMcastInterface*,
                                             size_t) {
    *handle = kTestHandle2;
    return kEtcPalErrOk;
  };

  create_sacn_dmx_merger_fake.custom_fake = [](const SacnDmxMergerConfig*, sacn_dmx_merger_t*) {
    return kEtcPalErrSys;
  };

  ++config.universe_id;
  EXPECT_EQ(sacn_merge_receiver_create(&config, &handle, nullptr, 0u), kEtcPalErrSys);

  EXPECT_EQ(get_num_merge_receivers(), 1u);

  EXPECT_EQ(destroy_sacn_receiver_fake.call_count, 1u);
  EXPECT_EQ(destroy_sacn_dmx_merger_fake.call_count, 0u);
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
    EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, static_cast<sacn_remote_source_t>(i), false),
              kEtcPalErrOk);
  }

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), kNumSources);

  EXPECT_EQ(sacn_merge_receiver_change_universe(handle, kTestUniverse + 1u), kEtcPalErrOk);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);

  EXPECT_EQ(change_sacn_receiver_universe_fake.call_count, 1u);
  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, kNumSources);
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

TEST_F(TestMergeReceiver, UniverseDataAddsPapSourceAfterSampling)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver, nullptr);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid = etcpal::Uuid::V4();

  RunUniverseData(1u, cid, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t handle, uint16_t universe, const uint8_t*,
                                      const sacn_dmx_merger_source_t*, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
  };

  RunUniverseData(1u, cid, 0x00, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, UniverseDataAddsNoPapSourceAfterSampling)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver, nullptr);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t handle, uint16_t universe, const uint8_t*,
                                      const sacn_dmx_merger_source_t*, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
  };

  RunUniverseData(1u, etcpal::Uuid::V4(), 0x00, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, PendingSourceBlocksUniverseData)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver, nullptr);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid1 = etcpal::Uuid::V4();
  etcpal::Uuid cid2 = etcpal::Uuid::V4();

  RunUniverseData(1u, cid1, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(2u, cid2, 0x00, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(2u, cid2, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(2u, cid2, 0x00, {0x03u, 0x04u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(1u, cid1, 0x00, {0x05u, 0x06u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  etcpal::Uuid cid3 = etcpal::Uuid::V4();

  RunUniverseData(3u, cid3, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunUniverseData(1u, cid1, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunUniverseData(1u, cid1, 0xDD, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunUniverseData(2u, cid2, 0x00, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  RunUniverseData(3u, cid3, 0x00, {0x07u, 0x08u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);

  RunUniverseData(1u, cid1, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  RunUniverseData(1u, cid1, 0xDD, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 4u);
  RunUniverseData(2u, cid2, 0x00, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 5u);
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 6u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 7u);
}

TEST_F(TestMergeReceiver, MultiplePendingSourcesBlockUniverseData)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver, nullptr);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid1 = etcpal::Uuid::V4();
  etcpal::Uuid cid2 = etcpal::Uuid::V4();
  etcpal::Uuid cid3 = etcpal::Uuid::V4();

  RunUniverseData(1u, cid1, 0xDD, {0xFFu, 0xFFu});
  RunUniverseData(2u, cid2, 0xDD, {0xFFu, 0xFFu});
  RunUniverseData(3u, cid3, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(1u, cid1, 0x00, {0x01u, 0x02u});
  RunUniverseData(2u, cid2, 0x00, {0x03u, 0x04u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(3u, cid3, 0x00, {0x05u, 0x06u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, SamplingPeriodBlocksUniverseData)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  RunSamplingStarted();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver, nullptr);

  etcpal::Uuid cid1 = etcpal::Uuid::V4();
  etcpal::Uuid cid2 = etcpal::Uuid::V4();

  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(1u, cid1, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(1u, cid1, 0xDD, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(2u, cid2, 0x00, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunSamplingEnded();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  RunUniverseData(1u, cid1, 0x00, {0x05u, 0x06u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);
  RunUniverseData(1u, cid1, 0xDD, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  RunUniverseData(2u, cid2, 0x00, {0x07u, 0x08u});
  EXPECT_EQ(universe_data_fake.call_count, 4u);
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 5u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 6u);
}

TEST_F(TestMergeReceiver, UniverseDataHandlesSourcesLost)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  etcpal::Uuid cid1 = etcpal::Uuid::V4();
  etcpal::Uuid cid2 = etcpal::Uuid::V4();
  etcpal::Uuid cid3 = etcpal::Uuid::V4();
  etcpal::Uuid cid4 = etcpal::Uuid::V4();
  etcpal::Uuid cid5 = etcpal::Uuid::V4();
  etcpal::Uuid cid6 = etcpal::Uuid::V4();
  etcpal::Uuid cid7 = etcpal::Uuid::V4();

  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(1u, cid1, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunUniverseData(2u, cid2, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);
  RunUniverseData(3u, cid3, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  RunUniverseData(4u, cid4, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 4u);
  RunUniverseData(5u, cid5, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 5u);
  RunUniverseData(6u, cid6, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 6u);
  RunUniverseData(7u, cid7, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 7u);

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver, nullptr);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 7u);

  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 0u);

  RunSourcesLost({{(sacn_remote_source_t)1u, cid1},
                  {(sacn_remote_source_t)2u, cid2},
                  {(sacn_remote_source_t)3u, cid3},
                  {(sacn_remote_source_t)4u, cid4}});

  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 4u);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(universe_data_fake.call_count, 8u);

  RunSourcesLost(
      {{(sacn_remote_source_t)5u, cid5}, {(sacn_remote_source_t)6u, cid6}, {(sacn_remote_source_t)7u, cid7}});

  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 7u);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(universe_data_fake.call_count, 9u);
}

TEST_F(TestMergeReceiver, UniverseDataHandlesPapLost)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  etcpal::Uuid cid = etcpal::Uuid::V4();

  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(1u, cid, 0xDD, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(1u, cid, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  EXPECT_EQ(remove_sacn_dmx_merger_paps_fake.call_count, 0u);
  RunPapLost(1u, cid);
  EXPECT_EQ(remove_sacn_dmx_merger_paps_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 2u);
}

TEST_F(TestMergeReceiver, UniverseNonDmxWorks)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  static const etcpal::Uuid kCid = etcpal::Uuid::V4();

  universe_non_dmx_fake.custom_fake = [](sacn_merge_receiver_t handle, uint16_t universe,
                                         const EtcPalSockAddr* source_addr, const SacnHeaderData* header,
                                         const uint8_t*, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(source_addr->port, kTestSourceAddr.port);
    EXPECT_EQ(etcpal_ip_cmp(&source_addr->ip, &kTestSourceAddr.ip), 0);
    EXPECT_EQ(ETCPAL_UUID_CMP(&header->cid, &kCid.get()), 0);
    EXPECT_EQ(strcmp(header->source_name, kTestHeaderData.source_name), 0);
    EXPECT_EQ(header->universe_id, kTestUniverse);
    EXPECT_EQ(header->priority, kTestPriority);
    EXPECT_EQ(header->preview, kTestHeaderData.preview);
    EXPECT_EQ(header->start_code, 0x77);
    EXPECT_EQ(header->slot_count, 2u);
  };

  RunSamplingStarted();

  EXPECT_EQ(universe_non_dmx_fake.call_count, 0u);
  RunUniverseData(1u, kCid, 0x77, {0x12u, 0x34u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 1u);
  RunUniverseData(1u, kCid, 0xDD, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 1u);
  RunUniverseData(1u, kCid, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 1u);
  RunUniverseData(1u, kCid, 0x77, {0x56u, 0x78u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 2u);

  RunSamplingEnded();

  RunUniverseData(1u, kCid, 0x77, {0x12u, 0x34u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 3u);
  RunUniverseData(1u, kCid, 0xDD, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 3u);
  RunUniverseData(1u, kCid, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 3u);
  RunUniverseData(1u, kCid, 0x77, {0x56u, 0x78u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 4u);
}

TEST_F(TestMergeReceiver, SourceLimitExceededWorks)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  source_limit_exceeded_fake.custom_fake = [](sacn_merge_receiver_t handle, uint16_t universe, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
  };

  RunSamplingStarted();

  EXPECT_EQ(source_limit_exceeded_fake.call_count, 0u);
  RunSourceLimitExceeded();
  EXPECT_EQ(source_limit_exceeded_fake.call_count, 1u);
  RunSourceLimitExceeded();
  EXPECT_EQ(source_limit_exceeded_fake.call_count, 2u);
  RunSourceLimitExceeded();
  EXPECT_EQ(source_limit_exceeded_fake.call_count, 3u);

  RunSamplingEnded();

  RunSourceLimitExceeded();
  EXPECT_EQ(source_limit_exceeded_fake.call_count, 4u);
  RunSourceLimitExceeded();
  EXPECT_EQ(source_limit_exceeded_fake.call_count, 5u);
  RunSourceLimitExceeded();
  EXPECT_EQ(source_limit_exceeded_fake.call_count, 6u);
  RunSourceLimitExceeded();
  EXPECT_EQ(source_limit_exceeded_fake.call_count, 7u);
}

TEST_F(TestMergeReceiver, PapBlockedWhenUsePapDisabled)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  SacnMergeReceiverConfig config = kTestConfig;
  config.use_pap = false;
  EXPECT_EQ(sacn_merge_receiver_create(&config, &handle, nullptr, 0u), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver, nullptr);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid = etcpal::Uuid::V4();

  RunUniverseData(1u, cid, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(1u, cid, 0x00, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  EXPECT_EQ(remove_sacn_dmx_merger_paps_fake.call_count, 0u);
  RunPapLost(1u, cid);
  EXPECT_EQ(remove_sacn_dmx_merger_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}
