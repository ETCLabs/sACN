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

FAKE_VOID_FUNC(universe_data, sacn_merge_receiver_t, uint16_t, const uint8_t*, const sacn_source_id_t*, void*);
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
    etcpal::Uuid::V4().get(), {'\0'}, kTestUniverse, kTestPriority, false, 0x00, DMX_ADDRESS_COUNT};

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

    sacn_receiver_create_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t* handle, SacnMcastInterface*,
                                               size_t) {
      *handle = kTestHandle;
      return kEtcPalErrOk;
    };

    sacn_dmx_merger_create_fake.custom_fake = [](const SacnDmxMergerConfig*, sacn_dmx_merger_t* handle) {
      *handle = kTestHandle;
      return kEtcPalErrOk;
    };

    sacn_dmx_merger_add_source_fake.custom_fake = [](sacn_dmx_merger_t, sacn_source_id_t* source_id) {
      static sacn_source_id_t next_source_id = 0u;
      *source_id = next_source_id++;
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

  void RunUniverseData(const etcpal::Uuid& source_cid, uint8_t start_code, const std::vector<uint8_t>& pdata,
                       uint8_t priority = kTestPriority)
  {
    SacnHeaderData header = kTestHeaderData;
    header.cid = source_cid.get();
    header.priority = priority;
    header.start_code = start_code;
    header.slot_count = static_cast<uint16_t>(pdata.size());
    merge_receiver_universe_data(kTestHandle, &kTestSourceAddr, &header, pdata.data(), false, nullptr);
  }

  void RunSamplingStarted() { merge_receiver_sampling_started(kTestHandle, kTestUniverse, nullptr); }
  void RunSamplingEnded() { merge_receiver_sampling_ended(kTestHandle, kTestUniverse, nullptr); }

  void RunSourcesLost(const std::vector<etcpal::Uuid>& cids)
  {
    std::vector<SacnLostSource> lost_sources;
    lost_sources.reserve(cids.size());
    std::transform(cids.begin(), cids.end(), std::back_inserter(lost_sources), [](const etcpal::Uuid& cid) {
      // clang-format off
        SacnLostSource lost_source = {
          cid.get(),
          {'\0'},
          true
        };
      // clang-format on

      return lost_source;
    });

    merge_receiver_sources_lost(kTestHandle, kTestUniverse, lost_sources.data(), lost_sources.size(), nullptr);
  }

  void RunPapLost(const etcpal::Uuid& cid)
  {
    SacnRemoteSource source = {cid.get(), {'\0'}};
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
    EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, static_cast<sacn_source_id_t>(i),
                                             &etcpal::Uuid::V4().get(), false),
              kEtcPalErrOk);
  }

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->cids_from_ids), kNumSources);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), kNumSources);

  EXPECT_EQ(sacn_merge_receiver_change_universe(handle, kTestUniverse + 1u), kEtcPalErrOk);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->cids_from_ids), 0u);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);

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

TEST_F(TestMergeReceiver, GetSourceIdWorks)
{
  static constexpr sacn_source_id_t kTestSourceId = 123u;
  static const EtcPalUuid kTestCid = etcpal::Uuid::V4().get();

  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  EXPECT_EQ(sacn_merge_receiver_get_source_id(handle, &kTestCid), SACN_DMX_MERGER_SOURCE_INVALID);

  SacnMergeReceiver* merge_receiver = nullptr;
  ASSERT_EQ(lookup_merge_receiver(handle, &merge_receiver, nullptr), kEtcPalErrOk);
  EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, kTestSourceId, &kTestCid, false), kEtcPalErrOk);

  EXPECT_EQ(sacn_merge_receiver_get_source_id(handle, &kTestCid), kTestSourceId);
}

TEST_F(TestMergeReceiver, GetSourceCidWorks)
{
  static constexpr sacn_source_id_t kTestSourceId = 123u;
  static const EtcPalUuid kTestCid = etcpal::Uuid::V4().get();

  EtcPalUuid cid_result = kEtcPalNullUuid;

  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr, 0u), kEtcPalErrOk);

  EXPECT_EQ(sacn_merge_receiver_get_source_cid(handle, kTestSourceId, &cid_result), kEtcPalErrNotFound);

  SacnMergeReceiver* merge_receiver = nullptr;
  ASSERT_EQ(lookup_merge_receiver(handle, &merge_receiver, nullptr), kEtcPalErrOk);
  EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, kTestSourceId, &kTestCid, false), kEtcPalErrOk);

  EXPECT_EQ(sacn_merge_receiver_get_source_cid(handle, kTestSourceId, &cid_result), kEtcPalErrOk);
  EXPECT_EQ(ETCPAL_UUID_CMP(&cid_result, &kTestCid), 0);
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
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid = etcpal::Uuid::V4();

  RunUniverseData(cid, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t handle, uint16_t universe, const uint8_t*,
                                      const sacn_source_id_t*, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
  };

  RunUniverseData(cid, 0x00, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 1u);
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
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t handle, uint16_t universe, const uint8_t*,
                                      const sacn_source_id_t*, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
  };

  RunUniverseData(etcpal::Uuid::V4(), 0x00, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 0u);
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
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid1 = etcpal::Uuid::V4();
  etcpal::Uuid cid2 = etcpal::Uuid::V4();

  RunUniverseData(cid1, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(cid2, 0x00, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 2u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(cid2, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 2u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(cid2, 0x00, {0x03u, 0x04u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 2u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 2u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 2u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(cid1, 0x00, {0x05u, 0x06u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 2u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 3u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 3u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
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
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid1 = etcpal::Uuid::V4();
  etcpal::Uuid cid2 = etcpal::Uuid::V4();
  etcpal::Uuid cid3 = etcpal::Uuid::V4();

  RunUniverseData(cid1, 0xDD, {0xFFu, 0xFFu});
  RunUniverseData(cid2, 0xDD, {0xFFu, 0xFFu});
  RunUniverseData(cid3, 0xDD, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 3u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(cid1, 0x00, {0x01u, 0x02u});
  RunUniverseData(cid2, 0x00, {0x03u, 0x04u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 3u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 2u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 2u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(cid3, 0x00, {0x05u, 0x06u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 3u);
  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 3u);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 3u);
  EXPECT_EQ(sacn_dmx_merger_update_paps_fake.call_count, 3u);
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
  RunUniverseData(cid1, 0x00, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(cid1, 0xDD, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(cid2, 0x00, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunSourcesLost({cid2});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunPapLost(cid1);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunSamplingEnded();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  RunUniverseData(cid1, 0x00, {0x05u, 0x06u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);
  RunUniverseData(cid1, 0xDD, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  RunUniverseData(cid2, 0x00, {0x07u, 0x08u});
  EXPECT_EQ(universe_data_fake.call_count, 4u);
  RunSourcesLost({cid2});
  EXPECT_EQ(universe_data_fake.call_count, 5u);
  RunPapLost(cid1);
  EXPECT_EQ(universe_data_fake.call_count, 6u);
}
