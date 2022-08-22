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

#include "sacn/merge_receiver.h"

#include <limits>
#include <optional>
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

FAKE_VOID_FUNC(universe_data, sacn_merge_receiver_t, const SacnRecvMergedData*, void*);
FAKE_VOID_FUNC(universe_non_dmx, sacn_merge_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
               const SacnRecvUniverseData*, void*);
FAKE_VOID_FUNC(sources_lost, sacn_merge_receiver_t, uint16_t, const SacnLostSource*, size_t, void*);
FAKE_VOID_FUNC(sampling_started, sacn_merge_receiver_t, uint16_t, void*);
FAKE_VOID_FUNC(sampling_ended, sacn_merge_receiver_t, uint16_t, void*);
FAKE_VOID_FUNC(source_limit_exceeded, sacn_merge_receiver_t, uint16_t, void*);

static constexpr uint16_t kTestUniverse = 123u;
static constexpr uint8_t kTestPriority = 100u;
static constexpr int kTestHandle = 4567u;
static constexpr int kTestHandle2 = 1234u;
static constexpr SacnMergeReceiverConfig kTestConfig = {
    kTestUniverse,
    {universe_data, universe_non_dmx, sources_lost, sampling_started, sampling_ended, source_limit_exceeded, NULL},
    {1, DMX_ADDRESS_COUNT},
    SACN_RECEIVER_INFINITE_SOURCES,
    true,
    kSacnIpV4AndIpV6};
static const EtcPalSockAddr kTestSourceAddr = {SACN_PORT, etcpal::IpAddr::FromString("10.101.1.1").get()};
static const SacnRemoteSource kTestRemoteSource = {0u, etcpal::Uuid::V4().get(), {'\0'}};
static const std::string kTestSourceName = kTestRemoteSource.name;
static const SacnRecvUniverseData kTestUniverseData = {kTestUniverse,      kTestPriority,          false,  false,
                                                       SACN_STARTCODE_DMX, {1, DMX_ADDRESS_COUNT}, nullptr};

class TestMergeReceiver : public ::testing::Test
{
public:
  static void SetSourceCountToExpect(const std::optional<size_t>& count) { source_count_to_expect_ = count; }
  static void CheckSourceCount(size_t count)
  {
    if (source_count_to_expect_)
    {
      EXPECT_EQ(count, *source_count_to_expect_);
    }
  }

protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_dmx_merger_reset_all_fakes();
    sacn_receiver_reset_all_fakes();

    RESET_FAKE(universe_data);
    RESET_FAKE(universe_non_dmx);
    RESET_FAKE(sources_lost);
    RESET_FAKE(sampling_started);
    RESET_FAKE(sampling_ended);
    RESET_FAKE(source_limit_exceeded);

    SetSourceCountToExpect(std::nullopt);

    universe_data_fake.custom_fake = [](sacn_merge_receiver_t, const SacnRecvMergedData* merged_data, void*) {
      CheckSourceCount(merged_data->num_active_sources);
    };

    create_sacn_receiver_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t* handle,
                                               const SacnNetintConfig*, const SacnReceiverInternalCallbacks*) {
      *handle = kTestHandle;
      return kEtcPalErrOk;
    };

    create_sacn_dmx_merger_fake.custom_fake = [](const SacnDmxMergerConfig*, sacn_dmx_merger_t* handle) {
      *handle = kTestHandle;
      return kEtcPalErrOk;
    };

    ASSERT_EQ(sacn_receiver_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_merge_receiver_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_merge_receiver_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_merge_receiver_deinit();
    sacn_merge_receiver_mem_deinit();
    sacn_receiver_mem_deinit();
  }

  SacnMergeReceiverSource ConstructSource(sacn_remote_source_t source_handle, const etcpal::SockAddr& source_addr,
                                          const etcpal::Uuid& source_cid, const std::string& source_name)
  {
    SacnMergeReceiverSource res;
    res.handle = source_handle;
    res.cid = source_cid.get();
    memset(res.name, '\0', SACN_SOURCE_NAME_MAX_LEN);
    memcpy(res.name, source_name.c_str(), source_name.length());
    res.addr = source_addr.get();
    return res;
  }

  void RunUniverseData(const SacnMergeReceiverSource& source, uint8_t start_code, const std::vector<uint8_t>& pdata,
                       uint8_t priority = kTestPriority, sacn_receiver_t receiver_handle = kTestHandle)
  {
    SacnRemoteSource remote_source = kTestRemoteSource;
    SacnRecvUniverseData universe_data = kTestUniverseData;
    remote_source.cid = source.cid;
    memcpy(remote_source.name, source.name, SACN_SOURCE_NAME_MAX_LEN);
    remote_source.handle = source.handle;
    universe_data.priority = priority;
    universe_data.start_code = start_code;
    universe_data.slot_range.address_count = static_cast<uint16_t>(pdata.size());
    universe_data.values = pdata.data();
    merge_receiver_universe_data(receiver_handle, &source.addr, &remote_source, &universe_data, 0u);
  }

  void RunUniverseData(sacn_remote_source_t source_handle, const etcpal::Uuid& source_cid, uint8_t start_code,
                       const std::vector<uint8_t>& pdata, uint8_t priority = kTestPriority,
                       sacn_receiver_t receiver_handle = kTestHandle)
  {
    RunUniverseData(ConstructSource(source_handle, kTestSourceAddr, source_cid, kTestSourceName), start_code, pdata,
                    priority, receiver_handle);
  }

  void RunSamplingStarted(sacn_receiver_t receiver_handle = kTestHandle)
  {
    merge_receiver_sampling_started(receiver_handle, kTestUniverse, 0u);
  }

  void RunSamplingEnded(sacn_receiver_t receiver_handle = kTestHandle)
  {
    merge_receiver_sampling_ended(receiver_handle, kTestUniverse, 0u);
  }

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

    merge_receiver_sources_lost(kTestHandle, kTestUniverse, lost_sources.data(), lost_sources.size(), 0u);
  }

  void RunPapLost(sacn_remote_source_t handle, const etcpal::Uuid& cid)
  {
    SacnRemoteSource source = {handle, cid.get(), {'\0'}};
    merge_receiver_pap_lost(kTestHandle, kTestUniverse, &source, 0u);
  }

  void RunSourceLimitExceeded() { merge_receiver_source_limit_exceeded(kTestHandle, kTestUniverse, 0u); }

  static std::optional<size_t> source_count_to_expect_;
};

std::optional<size_t> TestMergeReceiver::source_count_to_expect_;

bool operator==(const SacnMergeReceiverSource& s1, const SacnMergeReceiverSource& s2)
{
  if (s1.handle != s2.handle)
    return false;
  if (memcmp(s1.cid.data, s2.cid.data, ETCPAL_UUID_BYTES) != 0)
    return false;
  if (memcmp(s1.name, s2.name, SACN_SOURCE_NAME_MAX_LEN) != 0)
    return false;
  return etcpal_ip_and_port_equal(&s1.addr, &s2.addr);
}

TEST_F(TestMergeReceiver, CreateWorks)
{
  SacnMergeReceiverConfig config = kTestConfig;
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&config, &handle, nullptr), kEtcPalErrOk);

  EXPECT_EQ(handle, kTestHandle);
  EXPECT_EQ(create_sacn_receiver_fake.call_count, 1u);
  EXPECT_EQ(create_sacn_dmx_merger_fake.call_count, 1u);

  SacnMergeReceiver* merge_receiver = NULL;
  ASSERT_EQ(lookup_merge_receiver(handle, &merge_receiver), kEtcPalErrOk);
  EXPECT_EQ(merge_receiver->merge_receiver_handle, handle);
  EXPECT_EQ(merge_receiver->merger_handle, kTestHandle);
  EXPECT_TRUE(merge_receiver->use_pap);
  EXPECT_EQ(get_num_merge_receivers(), 1u);

  // Now test failure cleanup
  create_sacn_receiver_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t* handle,
                                             const SacnNetintConfig*, const SacnReceiverInternalCallbacks*) {
    *handle = kTestHandle2;
    return kEtcPalErrOk;
  };

  create_sacn_dmx_merger_fake.custom_fake = [](const SacnDmxMergerConfig*, sacn_dmx_merger_t*) {
    return kEtcPalErrSys;
  };

  ++config.universe_id;
  EXPECT_EQ(sacn_merge_receiver_create(&config, &handle, nullptr), kEtcPalErrSys);

  EXPECT_EQ(get_num_merge_receivers(), 1u);

  EXPECT_EQ(destroy_sacn_receiver_fake.call_count, 1u);
  EXPECT_EQ(destroy_sacn_dmx_merger_fake.call_count, 0u);
}

TEST_F(TestMergeReceiver, DestroyWorks)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);
  EXPECT_EQ(get_num_merge_receivers(), 1u);

  EXPECT_EQ(sacn_merge_receiver_destroy(handle), kEtcPalErrOk);
  EXPECT_EQ(get_num_merge_receivers(), 0u);
}

TEST_F(TestMergeReceiver, ChangeUniverseWorks)
{
  static constexpr size_t kNumSources = 5u;

  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  SacnMergeReceiver* merge_receiver = nullptr;
  ASSERT_EQ(lookup_merge_receiver(handle, &merge_receiver), kEtcPalErrOk);

  EtcPalSockAddr source_addr;
  SacnRemoteSource source_info;
  for (size_t i = 0u; i < kNumSources; ++i)
  {
    source_info.handle = static_cast<sacn_remote_source_t>(i);
    EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, &source_addr, &source_info, false), kEtcPalErrOk);
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

  sacn_receiver_reset_networking_per_receiver_fake.custom_fake =
      [](const SacnNetintConfig*, const SacnReceiverNetintList* netint_lists, size_t num_netint_lists) {
        for (size_t i = 0u; i < kNumNetintLists; ++i)
        {
          EXPECT_EQ(netint_lists[i].handle, (sacn_receiver_t)i);
          EXPECT_EQ(netint_lists[i].netints, nullptr);
          EXPECT_EQ(netint_lists[i].num_netints, 0u);
          EXPECT_EQ(num_netint_lists, kNumNetintLists);
        }

        return kEtcPalErrOk;
      };

  EXPECT_EQ(sacn_merge_receiver_reset_networking_per_receiver(nullptr, netint_lists, kNumNetintLists), kEtcPalErrOk);
  EXPECT_EQ(sacn_receiver_reset_networking_per_receiver_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, UniverseDataAddsPapSourceAfterSampling)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid = etcpal::Uuid::V4();

  RunUniverseData(1u, cid, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t handle, const SacnRecvMergedData* merged_data, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(merged_data->universe_id, kTestUniverse);
    CheckSourceCount(merged_data->num_active_sources);
  };

  SetSourceCountToExpect(1u);
  RunUniverseData(1u, cid, SACN_STARTCODE_DMX, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, UniverseDataAddsNoPapSourceAfterSampling)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t handle, const SacnRecvMergedData* merged_data, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(merged_data->universe_id, kTestUniverse);
    CheckSourceCount(merged_data->num_active_sources);
  };

  SetSourceCountToExpect(1u);
  RunUniverseData(1u, etcpal::Uuid::V4(), SACN_STARTCODE_DMX, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, PendingSourceBlocksUniverseData)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid1 = etcpal::Uuid::V4();
  etcpal::Uuid cid2 = etcpal::Uuid::V4();

  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(2u, cid2, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  SetSourceCountToExpect(2u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x05u, 0x06u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  etcpal::Uuid cid3 = etcpal::Uuid::V4();

  RunUniverseData(3u, cid3, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});

  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  SetSourceCountToExpect(2u);
  RunUniverseData(3u, cid3, SACN_STARTCODE_DMX, {0x07u, 0x08u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);

  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 4u);
  SetSourceCountToExpect(3u);
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 5u);
  SetSourceCountToExpect(2u);
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 6u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 7u);
}

TEST_F(TestMergeReceiver, MultiplePendingSourcesBlockUniverseData)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid1 = etcpal::Uuid::V4();
  etcpal::Uuid cid2 = etcpal::Uuid::V4();
  etcpal::Uuid cid3 = etcpal::Uuid::V4();

  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  RunUniverseData(2u, cid2, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  RunUniverseData(3u, cid3, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  SetSourceCountToExpect(3u);
  RunUniverseData(3u, cid3, SACN_STARTCODE_DMX, {0x05u, 0x06u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, SamplingPeriodBlocksUniverseData)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  RunSamplingStarted();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver);

  etcpal::Uuid cid1 = etcpal::Uuid::V4();
  etcpal::Uuid cid2 = etcpal::Uuid::V4();

  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  SetSourceCountToExpect(1u);
  RunSamplingEnded();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x05u, 0x06u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  SetSourceCountToExpect(2u);
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x07u, 0x08u});
  EXPECT_EQ(universe_data_fake.call_count, 4u);
  SetSourceCountToExpect(1u);
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 5u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 6u);
}

TEST_F(TestMergeReceiver, UniverseDataHandlesSourcesLost)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

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
  SetSourceCountToExpect(1u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  SetSourceCountToExpect(2u);
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);
  SetSourceCountToExpect(3u);
  RunUniverseData(3u, cid3, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  SetSourceCountToExpect(4u);
  RunUniverseData(4u, cid4, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 4u);
  SetSourceCountToExpect(5u);
  RunUniverseData(5u, cid5, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 5u);
  SetSourceCountToExpect(6u);
  RunUniverseData(6u, cid6, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 6u);
  SetSourceCountToExpect(7u);
  RunUniverseData(7u, cid7, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 7u);

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 7u);

  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 0u);

  SetSourceCountToExpect(3u);
  RunSourcesLost({{(sacn_remote_source_t)1u, cid1},
                  {(sacn_remote_source_t)2u, cid2},
                  {(sacn_remote_source_t)3u, cid3},
                  {(sacn_remote_source_t)4u, cid4}});

  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 4u);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(universe_data_fake.call_count, 8u);

  SetSourceCountToExpect(0u);
  RunSourcesLost(
      {{(sacn_remote_source_t)5u, cid5}, {(sacn_remote_source_t)6u, cid6}, {(sacn_remote_source_t)7u, cid7}});

  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 7u);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(universe_data_fake.call_count, 9u);
}

TEST_F(TestMergeReceiver, UniverseDataHandlesPapLost)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  etcpal::Uuid cid = etcpal::Uuid::V4();

  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(1u, cid, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  SetSourceCountToExpect(1u);
  RunUniverseData(1u, cid, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  EXPECT_EQ(remove_sacn_dmx_merger_pap_fake.call_count, 0u);
  RunPapLost(1u, cid);
  EXPECT_EQ(remove_sacn_dmx_merger_pap_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 2u);
}

TEST_F(TestMergeReceiver, UniverseNonDmxWorks)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  static const etcpal::Uuid kCid = etcpal::Uuid::V4();

  universe_non_dmx_fake.custom_fake = [](sacn_merge_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                         const SacnRemoteSource* source_info, const SacnRecvUniverseData* universe_data,
                                         void*) {
    EXPECT_EQ(receiver_handle, kTestHandle);
    EXPECT_EQ(universe_data->universe_id, kTestUniverse);
    EXPECT_EQ(source_addr->port, kTestSourceAddr.port);
    EXPECT_EQ(etcpal_ip_cmp(&source_addr->ip, &kTestSourceAddr.ip), 0);
    EXPECT_EQ(ETCPAL_UUID_CMP(&source_info->cid, &kCid.get()), 0);
    EXPECT_EQ(strcmp(source_info->name, kTestRemoteSource.name), 0);
    EXPECT_EQ(universe_data->universe_id, kTestUniverse);
    EXPECT_EQ(universe_data->priority, kTestPriority);
    EXPECT_EQ(universe_data->preview, kTestUniverseData.preview);
    EXPECT_EQ(universe_data->start_code, 0x77);
    EXPECT_EQ(universe_data->slot_range.address_count, 2);
  };

  RunSamplingStarted();

  EXPECT_EQ(universe_non_dmx_fake.call_count, 0u);
  RunUniverseData(1u, kCid, 0x77, {0x12u, 0x34u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 1u);
  RunUniverseData(1u, kCid, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 1u);
  RunUniverseData(1u, kCid, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 1u);
  RunUniverseData(1u, kCid, 0x77, {0x56u, 0x78u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 2u);

  RunSamplingEnded();

  RunUniverseData(1u, kCid, 0x77, {0x12u, 0x34u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 3u);
  RunUniverseData(1u, kCid, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 3u);
  RunUniverseData(1u, kCid, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 3u);
  RunUniverseData(1u, kCid, 0x77, {0x56u, 0x78u});
  EXPECT_EQ(universe_non_dmx_fake.call_count, 4u);
}

TEST_F(TestMergeReceiver, SourcesLostWorks)
{
  static const etcpal::Uuid kCid1 = etcpal::Uuid::V4();
  static const etcpal::Uuid kCid2 = etcpal::Uuid::V4();
  static constexpr sacn_remote_source_t kHandle1 = 1u;
  static constexpr sacn_remote_source_t kHandle2 = 2u;

  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  sources_lost_fake.custom_fake = [](sacn_merge_receiver_t handle, uint16_t universe,
                                     const SacnLostSource* lost_sources, size_t num_lost_sources, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);

    ASSERT_EQ(num_lost_sources, 2u);
    EXPECT_EQ(lost_sources[0].handle, kHandle1);
    EXPECT_EQ(lost_sources[1].handle, kHandle2);
    EXPECT_EQ(ETCPAL_UUID_CMP(&lost_sources[0].cid, &kCid1.get()), 0);
    EXPECT_EQ(ETCPAL_UUID_CMP(&lost_sources[1].cid, &kCid2.get()), 0);
  };

  RunUniverseData(1u, kCid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  RunUniverseData(2u, kCid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});
  EXPECT_EQ(sources_lost_fake.call_count, 0u);
  RunSourcesLost({{kHandle1, kCid1}, {kHandle2, kCid2}});
  EXPECT_EQ(sources_lost_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, SamplingStartedWorks)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  sampling_started_fake.custom_fake = [](sacn_merge_receiver_t handle, uint16_t universe, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
  };

  EXPECT_EQ(sampling_started_fake.call_count, 0u);
  RunSamplingStarted();
  EXPECT_EQ(sampling_started_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, SamplingEndedWorks)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  sampling_ended_fake.custom_fake = [](sacn_merge_receiver_t handle, uint16_t universe, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
  };

  RunSamplingStarted();

  EXPECT_EQ(sampling_ended_fake.call_count, 0u);
  RunSamplingEnded();
  EXPECT_EQ(sampling_ended_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, SourceLimitExceededWorks)
{
  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

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
  EXPECT_EQ(sacn_merge_receiver_create(&config, &handle, nullptr), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  etcpal::Uuid cid = etcpal::Uuid::V4();

  RunUniverseData(1u, cid, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  SetSourceCountToExpect(1u);
  RunUniverseData(1u, cid, SACN_STARTCODE_DMX, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  EXPECT_EQ(remove_sacn_dmx_merger_pap_fake.call_count, 0u);
  RunPapLost(1u, cid);
  EXPECT_EQ(remove_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, TracksSourceInfo)
{
  static const etcpal::SockAddr kSource1Addr = etcpal::SockAddr(etcpal::IpAddr::FromString("10.101.1.1"), 1u);
  static const etcpal::SockAddr kSource2Addr = etcpal::SockAddr(etcpal::IpAddr::FromString("10.101.2.2"), 2u);
  static const etcpal::Uuid kSource1Cid = etcpal::Uuid::V4();
  static const etcpal::Uuid kSource2Cid = etcpal::Uuid::V4();
  static const std::string kSource1Name = "Source 1 Name";
  static const std::string kSource2Name = "Source 2 Name";

  sacn_merge_receiver_t merge_receiver_handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &merge_receiver_handle, nullptr), kEtcPalErrOk);

  sacn_remote_source_t source_1_handle;
  EXPECT_EQ(add_remote_source_handle(&kSource1Cid.get(), &source_1_handle), kEtcPalErrOk);
  SacnMergeReceiverSource source1 = ConstructSource(source_1_handle, kSource1Addr, kSource1Cid, kSource1Name);

  sacn_remote_source_t source_2_handle;
  EXPECT_EQ(add_remote_source_handle(&kSource2Cid.get(), &source_2_handle), kEtcPalErrOk);
  SacnMergeReceiverSource source2 = ConstructSource(source_2_handle, kSource2Addr, kSource2Cid, kSource2Name);

  SacnMergeReceiverSource get_source_result;
  EXPECT_EQ(sacn_merge_receiver_get_source(merge_receiver_handle, source_1_handle, &get_source_result),
            kEtcPalErrNotFound);
  EXPECT_EQ(sacn_merge_receiver_get_source(merge_receiver_handle, source_2_handle, &get_source_result),
            kEtcPalErrNotFound);

  RunUniverseData(source1, SACN_STARTCODE_DMX, {0x12u, 0x34u});
  EXPECT_EQ(sacn_merge_receiver_get_source(merge_receiver_handle, source_1_handle, &get_source_result), kEtcPalErrOk);
  EXPECT_EQ(get_source_result, source1);

  RunUniverseData(source2, SACN_STARTCODE_DMX, {0x56u, 0x78u});
  EXPECT_EQ(sacn_merge_receiver_get_source(merge_receiver_handle, source_2_handle, &get_source_result), kEtcPalErrOk);
  EXPECT_EQ(get_source_result, source2);
}

TEST_F(TestMergeReceiver, DirectsDmxMergerOutputCorrectly)
{
  static const etcpal::Uuid kCid1 = etcpal::Uuid::V4();
  static const etcpal::Uuid kCid2 = etcpal::Uuid::V4();
  static constexpr sacn_remote_source_t kSourceHandle1 = 1u;
  static constexpr sacn_remote_source_t kSourceHandle2 = 2u;
  static constexpr uint8_t kLevel1 = 10u;
  static constexpr uint8_t kLevel2 = 20u;
  static constexpr sacn_receiver_t kReceiver2Handle = kTestHandle2;

  sacn_merge_receiver_t receiver_1 = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &receiver_1, nullptr), kEtcPalErrOk);

  // Create a second merge receiver for the majority of testing (it should still work even when the first merge receiver
  // is removed)
  static SacnDmxMergerConfig receiver_2_merger_config;
  create_sacn_dmx_merger_fake.custom_fake = [](const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle) {
    *handle = kTestHandle2;
    receiver_2_merger_config = *config;
    return kEtcPalErrOk;
  };

  create_sacn_receiver_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t* handle,
                                             const SacnNetintConfig*, const SacnReceiverInternalCallbacks*) {
    *handle = kReceiver2Handle;
    return kEtcPalErrOk;
  };

  sacn_merge_receiver_t receiver_2 = SACN_MERGE_RECEIVER_INVALID;
  SacnMergeReceiverConfig config_2 = kTestConfig;
  ++config_2.universe_id;
  EXPECT_EQ(sacn_merge_receiver_create(&config_2, &receiver_2, nullptr), kEtcPalErrOk);

  RunSamplingStarted(kReceiver2Handle);
  RunSamplingEnded(kReceiver2Handle);

  // Minimal DMX merger fake for testing output
  update_sacn_dmx_merger_levels_fake.custom_fake = [](sacn_dmx_merger_t, sacn_dmx_merger_source_t source,
                                                      const uint8_t* new_levels, size_t) {
    receiver_2_merger_config.levels[0] = new_levels[0];
    receiver_2_merger_config.owners[0] = source;  // Just assume the next source always wins
    return kEtcPalErrOk;
  };

  // Now test that the first source makes it to the merged data callback
  universe_data_fake.custom_fake = [](sacn_merge_receiver_t, const SacnRecvMergedData* merged_data, void*) {
    EXPECT_EQ(merged_data->levels[0], kLevel1);
    EXPECT_EQ(merged_data->owners[0], kSourceHandle1);
  };

  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(kSourceHandle1, kCid1, SACN_STARTCODE_DMX, {kLevel1}, kTestPriority, kReceiver2Handle);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  // Test that the second source makes it as well after removing the first merge receiver
  EXPECT_EQ(sacn_merge_receiver_destroy(receiver_1), kEtcPalErrOk);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t, const SacnRecvMergedData* merged_data, void*) {
    EXPECT_EQ(merged_data->levels[0], kLevel2);
    EXPECT_EQ(merged_data->owners[0], kSourceHandle2);
  };

  EXPECT_EQ(universe_data_fake.call_count, 1u);
  RunUniverseData(kSourceHandle2, kCid2, SACN_STARTCODE_DMX, {kLevel2}, kTestPriority, kReceiver2Handle);
  EXPECT_EQ(universe_data_fake.call_count, 2u);
}
