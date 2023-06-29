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
#include <unordered_set>
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
FAKE_VOID_FUNC(source_pap_lost, sacn_merge_receiver_t, uint16_t, const SacnRemoteSource*, void*);
FAKE_VOID_FUNC(source_limit_exceeded, sacn_merge_receiver_t, uint16_t, void*);

static constexpr uint16_t kTestUniverse = 123u;
static constexpr uint8_t kTestPriority = 100u;
static constexpr int kTestHandle = 4567u;
static constexpr int kTestHandle2 = 1234u;
static constexpr sacn_dmx_merger_t kInitialMergerHandle = 0;
static constexpr SacnMergeReceiverConfig kTestConfig = {
    kTestUniverse,
    {universe_data, universe_non_dmx, sources_lost, sampling_started, sampling_ended, source_pap_lost,
     source_limit_exceeded, NULL},
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
  static void SetActiveSourcesToExpect(const std::unordered_set<sacn_remote_source_t>& sources)
  {
    active_sources_to_expect_ = sources;
  }

  static void CheckActiveSources(const sacn_remote_source_t* sources, size_t num_sources)
  {
    if (active_sources_to_expect_)
    {
      std::unordered_set<sacn_remote_source_t> source_set;
      for (size_t i = 0; i < num_sources; ++i)
        source_set.insert(sources[i]);

      EXPECT_EQ(source_set, *active_sources_to_expect_);
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
    RESET_FAKE(source_pap_lost);
    RESET_FAKE(source_limit_exceeded);

    active_sources_to_expect_ = std::nullopt;

    universe_data_fake.custom_fake = [](sacn_merge_receiver_t, const SacnRecvMergedData* merged_data, void*) {
      CheckActiveSources(merged_data->active_sources, merged_data->num_active_sources);
    };

    create_sacn_receiver_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t* handle,
                                               const SacnNetintConfig*, const SacnReceiverInternalCallbacks*) {
      *handle = kTestHandle;
      return kEtcPalErrOk;
    };

    next_merger_handle_ = kInitialMergerHandle;
    create_sacn_dmx_merger_fake.custom_fake = [](const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle) {
      *handle = next_merger_handle_;
      ++next_merger_handle_;
      merger_configs_[*handle] = *config;
      return kEtcPalErrOk;
    };

    lookup_state_fake.custom_fake = [](sacn_dmx_merger_t, sacn_dmx_merger_source_t, MergerState**,
                                       SourceState** source_state) {
      *source_state = &dummy_dmx_merger_source_state_;
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
                       uint8_t priority = kTestPriority, sacn_receiver_t receiver_handle = kTestHandle,
                       bool sampling = false)
  {
    SacnRemoteSource remote_source = kTestRemoteSource;
    SacnRecvUniverseData universe_data = kTestUniverseData;
    remote_source.cid = source.cid;
    memcpy(remote_source.name, source.name, SACN_SOURCE_NAME_MAX_LEN);
    remote_source.handle = source.handle;
    universe_data.priority = priority;
    universe_data.is_sampling = sampling;
    universe_data.start_code = start_code;
    universe_data.slot_range.address_count = static_cast<uint16_t>(pdata.size());
    universe_data.values = pdata.data();
    merge_receiver_universe_data(receiver_handle, &source.addr, &remote_source, &universe_data, 0u);
  }

  void RunUniverseData(sacn_remote_source_t source_handle, const etcpal::Uuid& source_cid, uint8_t start_code,
                       const std::vector<uint8_t>& pdata, uint8_t priority = kTestPriority,
                       sacn_receiver_t receiver_handle = kTestHandle, bool sampling = false)
  {
    RunUniverseData(ConstructSource(source_handle, kTestSourceAddr, source_cid, kTestSourceName), start_code, pdata,
                    priority, receiver_handle, sampling);
  }

  void RunSamplingUniverseData(sacn_remote_source_t source_handle, const etcpal::Uuid& source_cid, uint8_t start_code,
                               const std::vector<uint8_t>& pdata, uint8_t priority = kTestPriority,
                               sacn_receiver_t receiver_handle = kTestHandle)
  {
    RunUniverseData(source_handle, source_cid, start_code, pdata, priority, receiver_handle, true);
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

  enum class RunPapLostExpectation
  {
    kPapLostCbCalled,
    kPapLostCbNotCalled
  };
  void RunPapLost(sacn_remote_source_t handle, const etcpal::Uuid& cid,
                  RunPapLostExpectation expectation = RunPapLostExpectation::kPapLostCbCalled)
  {
    auto old_call_count = source_pap_lost_fake.call_count;

    SacnRemoteSource source = {handle, cid.get(), {'\0'}};
    merge_receiver_pap_lost(kTestHandle, kTestUniverse, &source, 0u);

    if (expectation == RunPapLostExpectation::kPapLostCbCalled)
      EXPECT_EQ(source_pap_lost_fake.call_count, old_call_count + 1u);
    else
      EXPECT_EQ(source_pap_lost_fake.call_count, old_call_count);
  }

  void RunSourceLimitExceeded() { merge_receiver_source_limit_exceeded(kTestHandle, kTestUniverse, 0u); }

  static std::optional<std::unordered_set<sacn_remote_source_t>> active_sources_to_expect_;
  static SourceState dummy_dmx_merger_source_state_;
  static std::map<sacn_dmx_merger_t, SacnDmxMergerConfig> merger_configs_;
  static sacn_dmx_merger_t next_merger_handle_;
};

std::optional<std::unordered_set<sacn_remote_source_t>> TestMergeReceiver::active_sources_to_expect_;
SourceState TestMergeReceiver::dummy_dmx_merger_source_state_;
std::map<sacn_dmx_merger_t, SacnDmxMergerConfig> TestMergeReceiver::merger_configs_;
sacn_dmx_merger_t TestMergeReceiver::next_merger_handle_;

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
#if SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER
  EXPECT_EQ(create_sacn_dmx_merger_fake.call_count, 2u);
#else
  EXPECT_EQ(create_sacn_dmx_merger_fake.call_count, 1u);
#endif

  SacnMergeReceiver* merge_receiver = NULL;
  ASSERT_EQ(lookup_merge_receiver(handle, &merge_receiver), kEtcPalErrOk);
  EXPECT_EQ(merge_receiver->merge_receiver_handle, handle);
  EXPECT_EQ(merge_receiver->merger_handle, kInitialMergerHandle);
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

  create_sacn_receiver_fake.custom_fake = [](const SacnReceiverConfig*, sacn_receiver_t*, const SacnNetintConfig*,
                                             const SacnReceiverInternalCallbacks*) { return kEtcPalErrSys; };

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
    netint_lists[i].no_netints = false;
  }

  sacn_receiver_reset_networking_per_receiver_fake.custom_fake =
      [](const SacnNetintConfig*, const SacnReceiverNetintList* netint_lists, size_t num_netint_lists) {
        for (size_t i = 0u; i < kNumNetintLists; ++i)
        {
          EXPECT_EQ(netint_lists[i].handle, (sacn_receiver_t)i);
          EXPECT_EQ(netint_lists[i].netints, nullptr);
          EXPECT_EQ(netint_lists[i].num_netints, 0u);
          EXPECT_EQ(netint_lists[i].no_netints, false);
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

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t handle, const SacnRecvMergedData* merged_data, void*) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(merged_data->universe_id, kTestUniverse);
    CheckActiveSources(merged_data->active_sources, merged_data->num_active_sources);
  };

  SetActiveSourcesToExpect({1u});
  RunUniverseData(1u, cid, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 0u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  RunUniverseData(1u, cid, SACN_STARTCODE_DMX, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 2u);
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
    CheckActiveSources(merged_data->active_sources, merged_data->num_active_sources);
  };

  SetActiveSourcesToExpect({1u});
  RunUniverseData(1u, etcpal::Uuid::V4(), SACN_STARTCODE_DMX, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestMergeReceiver, UniverseDataFiresWithPendingSource)
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
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 2u);

  RunUniverseData(2u, cid2, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 3u);

  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 4u);

  SetActiveSourcesToExpect({1u, 2u});
  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x05u, 0x06u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 2u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 5u);

  etcpal::Uuid cid3 = etcpal::Uuid::V4();

  SetActiveSourcesToExpect({1u, 2u, 3u});
  RunUniverseData(3u, cid3, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 6u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 7u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 8u);
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 9u);
  SetActiveSourcesToExpect({1u, 3u});
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 10u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 11u);

  RunUniverseData(3u, cid3, SACN_STARTCODE_DMX, {0x07u, 0x08u});
  EXPECT_EQ(universe_data_fake.call_count, 12u);

  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 13u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 14u);
  SetActiveSourcesToExpect({1u, 2u, 3u});
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 15u);
  SetActiveSourcesToExpect({1u, 3u});
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 16u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 17u);
}

TEST_F(TestMergeReceiver, UniverseDataFiresWithMultiplePendingSources)
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
  EXPECT_EQ(universe_data_fake.call_count, 3u);

  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 5u);

  SetActiveSourcesToExpect({1u, 2u, 3u});
  RunUniverseData(3u, cid3, SACN_STARTCODE_DMX, {0x05u, 0x06u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 6u);
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
  RunSamplingUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunSamplingUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunSamplingUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x03u, 0x04u});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  SetActiveSourcesToExpect({1u});
  RunSamplingEnded();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x05u, 0x06u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  SetActiveSourcesToExpect({1u, 2u});
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x07u, 0x08u});
  EXPECT_EQ(universe_data_fake.call_count, 4u);
  SetActiveSourcesToExpect({1u});
  RunSourcesLost({{(sacn_remote_source_t)2u, cid2}});
  EXPECT_EQ(universe_data_fake.call_count, 5u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(universe_data_fake.call_count, 6u);

#if !SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER
  // If the sampling merger is disabled, then even the current sources should be interrupted during a sampling period.
  RunSamplingStarted();
  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x05u, 0x06u});  // Sampling = false, still shouldn't make it
  EXPECT_EQ(universe_data_fake.call_count, 6u);
  RunSamplingEnded();
  EXPECT_EQ(universe_data_fake.call_count, 7u);
#endif  // !SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER
}

#if SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER
// A sampling period could occur due to a networking reset. Current sources should continue to be included in ongoing
// merged data notifications, but the sources included on the new interfaces being sampled should be merged separately
// and should only be included in the merged data when their sampling period is over.
TEST_F(TestMergeReceiver, UniverseDataHandlesSamplingPeriodSources)
{
  static constexpr sacn_dmx_merger_t kPrimaryMergerHandle = kInitialMergerHandle;
  static constexpr sacn_dmx_merger_t kSamplingMergerHandle = kInitialMergerHandle + 1;
  static constexpr sacn_remote_source_t kNonSamplingSource = 1u;
  static constexpr sacn_remote_source_t kSamplingSource = 2u;

  add_sacn_dmx_merger_source_with_handle_fake.custom_fake = [](sacn_dmx_merger_t merger,
                                                               sacn_dmx_merger_source_t handle_to_use) {
    if (handle_to_use == kSamplingSource)
      EXPECT_EQ(merger, kSamplingMergerHandle);
    else  // handle_to_use == kNonSamplingSource
      EXPECT_EQ(merger, kPrimaryMergerHandle);

    return kEtcPalErrOk;
  };
  update_sacn_dmx_merger_levels_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                                      const uint8_t*, size_t) {
    if (source == kSamplingSource)
      EXPECT_EQ(merger, kSamplingMergerHandle);
    else  // source == kNonSamplingSource
      EXPECT_EQ(merger, kPrimaryMergerHandle);

    return kEtcPalErrOk;
  };

  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  // Initial source appears after the initial sampling period
  etcpal::Uuid cid1 = etcpal::Uuid::V4();

  SetActiveSourcesToExpect({kNonSamplingSource});
  RunUniverseData(kNonSamplingSource, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  // Let's say the network was reset - now we're in another sampling period on a new interface
  RunSamplingStarted();

  // A new source appears on the new interface, but shouldn't be included in universe data yet
  etcpal::Uuid cid2 = etcpal::Uuid::V4();
  RunSamplingUniverseData(kSamplingSource, cid2, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 2u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 2u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  // The existing source, however, will indicate it's not in the sampling period and should still be included
  RunUniverseData(kNonSamplingSource, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 3u);
  EXPECT_EQ(universe_data_fake.call_count, 2u);

  // The new source should be added to merged data once the sampling period ends
  remove_sacn_dmx_merger_source_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source) {
    EXPECT_EQ(merger, kSamplingMergerHandle);
    EXPECT_EQ(source, kSamplingSource);
    return kEtcPalErrOk;
  };
  add_sacn_dmx_merger_source_with_handle_fake.custom_fake = [](sacn_dmx_merger_t merger,
                                                               sacn_dmx_merger_source_t handle_to_use) {
    EXPECT_EQ(merger, kPrimaryMergerHandle);
    EXPECT_EQ(handle_to_use, kSamplingSource);
    return kEtcPalErrOk;
  };
  update_sacn_dmx_merger_levels_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t,
                                                      const uint8_t*, size_t) {
    EXPECT_EQ(merger, kPrimaryMergerHandle);
    return kEtcPalErrOk;
  };
  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 0u);
  SetActiveSourcesToExpect({kNonSamplingSource, kSamplingSource});
  RunSamplingEnded();
  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 3u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 4u);
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  RunUniverseData(kNonSamplingSource, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 5u);
  EXPECT_EQ(universe_data_fake.call_count, 4u);
}
#endif  // SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER

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
  SetActiveSourcesToExpect({1u});
  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  SetActiveSourcesToExpect({1u, 2u});
  RunUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);
  SetActiveSourcesToExpect({1u, 2u, 3u});
  RunUniverseData(3u, cid3, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  SetActiveSourcesToExpect({1u, 2u, 3u, 4u});
  RunUniverseData(4u, cid4, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 4u);
  SetActiveSourcesToExpect({1u, 2u, 3u, 4u, 5u});
  RunUniverseData(5u, cid5, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 5u);
  SetActiveSourcesToExpect({1u, 2u, 3u, 4u, 5u, 6u});
  RunUniverseData(6u, cid6, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 6u);
  SetActiveSourcesToExpect({1u, 2u, 3u, 4u, 5u, 6u, 7u});
  RunUniverseData(7u, cid7, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 7u);

  SacnMergeReceiver* merge_receiver = nullptr;
  lookup_merge_receiver(handle, &merge_receiver);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 7u);

  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 0u);

  SetActiveSourcesToExpect({5u, 6u, 7u});
  RunSourcesLost({{(sacn_remote_source_t)1u, cid1},
                  {(sacn_remote_source_t)2u, cid2},
                  {(sacn_remote_source_t)3u, cid3},
                  {(sacn_remote_source_t)4u, cid4}});

  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 4u);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 3u);
  EXPECT_EQ(universe_data_fake.call_count, 8u);

  SetActiveSourcesToExpect({});
  RunSourcesLost(
      {{(sacn_remote_source_t)5u, cid5}, {(sacn_remote_source_t)6u, cid6}, {(sacn_remote_source_t)7u, cid7}});

  EXPECT_EQ(remove_sacn_dmx_merger_source_fake.call_count, 7u);
  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
  EXPECT_EQ(universe_data_fake.call_count, 9u);
}

TEST_F(TestMergeReceiver, UniverseDataHandlesPapLost)
{
  static constexpr sacn_dmx_merger_t kPrimaryMergerHandle = kInitialMergerHandle;
#if SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER
  static constexpr sacn_dmx_merger_t kSamplingMergerHandle = kInitialMergerHandle + 1;
#endif

  sacn_merge_receiver_t handle = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &handle, nullptr), kEtcPalErrOk);

  RunSamplingStarted();
  RunSamplingEnded();

  etcpal::Uuid cid1 = etcpal::Uuid::V4();

  EXPECT_EQ(universe_data_fake.call_count, 0u);
  RunUniverseData(1u, cid1, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  SetActiveSourcesToExpect({1u});
  RunUniverseData(1u, cid1, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  EXPECT_EQ(universe_data_fake.call_count, 2u);

  remove_sacn_dmx_merger_pap_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t) {
    EXPECT_EQ(merger, kPrimaryMergerHandle);
    return kEtcPalErrOk;
  };

  EXPECT_EQ(remove_sacn_dmx_merger_pap_fake.call_count, 0u);
  RunPapLost(1u, cid1);
  EXPECT_EQ(remove_sacn_dmx_merger_pap_fake.call_count, 1u);
  EXPECT_EQ(universe_data_fake.call_count, 3u);

  // Now say a new interface is brought in and enters a sampling period - PAP lost should remove from the correct merger
#if SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER
  remove_sacn_dmx_merger_pap_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t) {
    EXPECT_EQ(merger, kSamplingMergerHandle);
    return kEtcPalErrOk;
  };
#endif

  etcpal::Uuid cid2 = etcpal::Uuid::V4();

  RunSamplingStarted();
  RunSamplingUniverseData(2u, cid2, SACN_STARTCODE_PRIORITY, {0xFFu, 0xFFu});
  RunSamplingUniverseData(2u, cid2, SACN_STARTCODE_DMX, {0x01u, 0x02u});
  RunPapLost(2u, cid2);
  EXPECT_EQ(remove_sacn_dmx_merger_pap_fake.call_count, 2u);
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

  SetActiveSourcesToExpect({1u});
  RunUniverseData(1u, cid, SACN_STARTCODE_DMX, {0x01u, 0x02u});

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 1u);
  EXPECT_EQ(add_sacn_dmx_merger_source_with_handle_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_levels_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(update_sacn_dmx_merger_pap_fake.call_count, 0u);
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  EXPECT_EQ(remove_sacn_dmx_merger_pap_fake.call_count, 0u);
  RunPapLost(1u, cid, RunPapLostExpectation::kPapLostCbNotCalled);
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
  static constexpr uint8_t kPriority1 = 30u;
  static constexpr uint8_t kPriority2 = 40u;
  static constexpr sacn_receiver_t kReceiver2Handle = kTestHandle2;

#if SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER
  static constexpr sacn_dmx_merger_t kReceiver2MergerHandle = kInitialMergerHandle + 2;
#else
  static constexpr sacn_dmx_merger_t kReceiver2MergerHandle = kInitialMergerHandle + 1;
#endif

  sacn_merge_receiver_t receiver_1 = SACN_MERGE_RECEIVER_INVALID;
  EXPECT_EQ(sacn_merge_receiver_create(&kTestConfig, &receiver_1, nullptr), kEtcPalErrOk);

  // Create a second merge receiver for the majority of testing (it should still work even when the first merge receiver
  // is removed)
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
  update_sacn_dmx_merger_levels_fake.custom_fake = [](sacn_dmx_merger_t, sacn_dmx_merger_source_t,
                                                      const uint8_t* new_levels, size_t) {
    merger_configs_[kReceiver2MergerHandle].levels[0] = new_levels[0];
    return kEtcPalErrOk;
  };
  update_sacn_dmx_merger_pap_fake.custom_fake = [](sacn_dmx_merger_t, sacn_dmx_merger_source_t source,
                                                   const uint8_t* new_priorities, size_t) {
    merger_configs_[kReceiver2MergerHandle].per_address_priorities[0] = new_priorities[0];
    merger_configs_[kReceiver2MergerHandle].owners[0] = source;  // Just assume the next source always wins
    return kEtcPalErrOk;
  };

  EXPECT_EQ(universe_data_fake.call_count, 0u);

  // Now test that the first source makes it to the merged data callback

  RunUniverseData(kSourceHandle1, kCid1, SACN_STARTCODE_DMX, {kLevel1}, kTestPriority, kReceiver2Handle);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t, const SacnRecvMergedData* merged_data, void*) {
    EXPECT_EQ(merged_data->levels[0], kLevel1);
    EXPECT_EQ(merged_data->priorities[0], kPriority1);
    EXPECT_EQ(merged_data->owners[0], kSourceHandle1);
  };
  RunUniverseData(kSourceHandle1, kCid1, SACN_STARTCODE_PRIORITY, {kPriority1}, kTestPriority, kReceiver2Handle);

  EXPECT_EQ(universe_data_fake.call_count, 2u);

  // Test that the second source makes it as well after removing the first merge receiver
  EXPECT_EQ(sacn_merge_receiver_destroy(receiver_1), kEtcPalErrOk);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t, const SacnRecvMergedData*, void*) {};
  RunUniverseData(kSourceHandle2, kCid2, SACN_STARTCODE_DMX, {kLevel2}, kTestPriority, kReceiver2Handle);

  universe_data_fake.custom_fake = [](sacn_merge_receiver_t, const SacnRecvMergedData* merged_data, void*) {
    EXPECT_EQ(merged_data->levels[0], kLevel2);
    EXPECT_EQ(merged_data->priorities[0], kPriority2);
    EXPECT_EQ(merged_data->owners[0], kSourceHandle2);
  };
  RunUniverseData(kSourceHandle2, kCid2, SACN_STARTCODE_PRIORITY, {kPriority2}, kTestPriority, kReceiver2Handle);

  EXPECT_EQ(universe_data_fake.call_count, 4u);
}
