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

#include "sacn/private/source_detector_state.h"

#include "etcpal_mock/common.h"
#include "etcpal_mock/timer.h"
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "sacn_mock/private/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/pdu.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestSourceDetectorState TestSourceDetectorStateDynamic
#else
#define TestSourceDetectorState TestSourceDetectorStateStatic
#endif

FAKE_VOID_FUNC(source_updated, sacn_remote_source_t, const EtcPalUuid*, const char*, const uint16_t*, size_t, void*);
FAKE_VOID_FUNC(source_expired, sacn_remote_source_t, const EtcPalUuid*, const char*, void*);
FAKE_VOID_FUNC(limit_exceeded, void*);

static const EtcPalSockAddr kTestSourceAddr = {SACN_PORT, etcpal::IpAddr::FromString("10.101.1.1").get()};
const std::string kTestName = "Test Name";

#if SACN_DYNAMIC_MEM
constexpr int kTestMaxSources = 3;
constexpr uint16_t kTestMaxUniverses = 2000u;
#else
constexpr int kTestMaxSources = SACN_SOURCE_DETECTOR_MAX_SOURCES;
constexpr uint16_t kTestMaxUniverses = SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE;
#endif

class TestSourceDetectorState : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();

    RESET_FAKE(source_updated);
    RESET_FAKE(source_expired);
    RESET_FAKE(limit_exceeded);

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_source_detector_state_init(), kEtcPalErrOk);

    CreateDetector(kTestMaxSources, kTestMaxUniverses);
  }

  void TearDown() override
  {
    DestroyDetector();

    sacn_source_detector_state_deinit();
    sacn_mem_deinit();
  }

  etcpal::Error CreateDetector(int source_count_max, int universes_per_source_max)
  {
    SacnSourceDetectorConfig config = SACN_SOURCE_DETECTOR_CONFIG_DEFAULT_INIT;
    config.callbacks.source_updated = source_updated;
    config.callbacks.source_expired = source_expired;
    config.callbacks.limit_exceeded = limit_exceeded;
    config.source_count_max = source_count_max;
    config.universes_per_source_max = universes_per_source_max;

    return add_sacn_source_detector(&config, nullptr, 0, &detector_);
  }

  void DestroyDetector()
  {
    remove_sacn_source_detector();
    detector_ = nullptr;
  }

  void ProcessUniverseDiscoveryPage(const etcpal::Uuid& cid, const std::vector<uint16_t>& complete_universe_list,
                                    uint8_t page_number)
  {
    size_t last_page = (complete_universe_list.size() / 512u);

    uint8_t buf[SACN_MTU] = {0};

    size_t num_universes = complete_universe_list.size() - (page_number * 512);
    if (num_universes > 512)
      num_universes = 512;

    CreateUniverseDiscoveryBuffer(&complete_universe_list.data()[page_number * 512], num_universes, page_number,
                                  (uint8_t)last_page, buf);

    SacnRecvThreadContext context;
    context.source_detector = detector_;

    handle_sacn_universe_discovery_packet(&context, buf, SACN_MTU, &cid.get(), &kTestSourceAddr, kTestName.c_str());
  }

  void ProcessUniverseDiscoveryPages(const etcpal::Uuid& cid, const std::vector<uint16_t>& complete_universe_list)
  {
    uint8_t last_page = static_cast<uint8_t>(complete_universe_list.size() / 512);

    for (uint8_t page = 0u; page <= last_page; ++page)
      ProcessUniverseDiscoveryPage(cid, complete_universe_list, page);
  }

  void CreateUniverseDiscoveryBuffer(const uint16_t* universes, size_t num_universes, uint8_t page, uint8_t last,
                                     uint8_t* buffer)
  {
    uint8_t* pcur = buffer;
    ACN_PDU_SET_V_FLAG(*pcur);
    ACN_PDU_SET_H_FLAG(*pcur);
    ACN_PDU_SET_D_FLAG(*pcur);
    ACN_PDU_PACK_NORMAL_LEN(pcur, (num_universes * 2) + 8);

    pcur += 2;
    etcpal_pack_u32b(pcur, VECTOR_UNIVERSE_DISCOVERY_UNIVERSE_LIST);

    pcur += 4;
    *pcur = page;

    ++pcur;
    *pcur = last;

    ++pcur;
    for (size_t i = 0; i < num_universes; ++i)
    {
      etcpal_pack_u16b(pcur, universes[i]);
      pcur += 2;
    }
  }

  void ProcessSourceDetector()
  {
    SacnRecvThreadContext context;
    context.source_detector = detector_;

    process_source_detector(&context);
  }

  SacnSourceDetector* detector_{nullptr};
};

TEST_F(TestSourceDetectorState, SourceUpdatedWorks)
{
  static std::vector<uint16_t> universe_list_1, universe_list_2;
  static etcpal::Uuid test_cid;
  static constexpr uint16_t kNumUniverses = 1000u;

  universe_list_1.clear();
  universe_list_2.clear();

  for (uint16_t universe = 0u; universe < kNumUniverses; ++universe)
    universe_list_1.push_back(universe);
  for (uint16_t universe = kNumUniverses; universe < (kNumUniverses * 2u); ++universe)
    universe_list_2.push_back(universe);

  test_cid = etcpal::Uuid::V4();

  source_updated_fake.custom_fake = [](sacn_remote_source_t, const EtcPalUuid* cid, const char* name,
                                       const uint16_t* sourced_universes, size_t num_sourced_universes, void* context) {
    EXPECT_EQ(ETCPAL_UUID_CMP(cid, &test_cid.get()), 0);
    EXPECT_EQ(strcmp(name, kTestName.c_str()), 0);

    for (size_t i = 0u; i < num_sourced_universes; ++i)
      EXPECT_EQ(sourced_universes[i], universe_list_1[i]);

    EXPECT_EQ(num_sourced_universes, kNumUniverses);
    EXPECT_EQ(context, nullptr);
  };

  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list_1, 0u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list_1, 1u);
  EXPECT_EQ(source_updated_fake.call_count, 1u);

  source_updated_fake.custom_fake = [](sacn_remote_source_t, const EtcPalUuid* cid, const char* name,
                                       const uint16_t* sourced_universes,
                                       size_t num_sourced_universes, void* context) {
    EXPECT_EQ(ETCPAL_UUID_CMP(cid, &test_cid.get()), 0);
    EXPECT_EQ(strcmp(name, kTestName.c_str()), 0);

    for (size_t i = 0u; i < num_sourced_universes; ++i)
      EXPECT_EQ(sourced_universes[i], universe_list_2[i]);

    EXPECT_EQ(num_sourced_universes, kNumUniverses);
    EXPECT_EQ(context, nullptr);
  };

  ProcessUniverseDiscoveryPage(test_cid, universe_list_2, 1u);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list_2, 0u);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list_2, 1u);
  EXPECT_EQ(source_updated_fake.call_count, 2u);
}

TEST_F(TestSourceDetectorState, SourceUpdatedFiltersDroppedLists)
{
  static std::vector<uint16_t> universe_list;
  static etcpal::Uuid test_cid;
  static constexpr uint16_t kNumUniverses = 2000u;

  universe_list.clear();
  for (uint16_t universe = 0u; universe < kNumUniverses; ++universe)
    universe_list.push_back(universe);

  source_updated_fake.custom_fake = [](sacn_remote_source_t, const EtcPalUuid*, const char*,
                                       const uint16_t* sourced_universes,
                                       size_t num_sourced_universes, void*) {
    for (size_t i = 0u; i < num_sourced_universes; ++i)
      EXPECT_EQ(sourced_universes[i], universe_list[i]);

    EXPECT_EQ(num_sourced_universes, kNumUniverses);
  };

  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 0u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 2u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 3u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 0u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 1u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 3u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 1u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 2u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 3u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 0u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 3u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 0u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 1u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 2u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid, universe_list, 3u);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
}

TEST_F(TestSourceDetectorState, SourceExpiredWorksAllAtOnce)
{
  static constexpr unsigned int kNumSources = 3u;
  static etcpal::Uuid test_cids[kNumSources];

  std::vector<uint16_t> universe_list = {1u, 2u, 3u};

  EtcPalUuid cid = kEtcPalNullUuid;
  for (unsigned int i = 0u; i < kNumSources; ++i)
  {
    etcpal_pack_u32b(cid.data, i);  // Increasing sequence of CIDs, so expired notifies in the same order.
    test_cids[i] = cid;
    ProcessUniverseDiscoveryPages(test_cids[i], universe_list);
    etcpal_getms_fake.return_val += 200u;
  }

  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL * 2u);

  static unsigned int index = 0u;
  source_expired_fake.custom_fake = [](sacn_remote_source_t, const EtcPalUuid* cid, const char* name, void* context) {
    EXPECT_EQ(ETCPAL_UUID_CMP(cid, &test_cids[index].get()), 0);
    EXPECT_EQ(strcmp(name, kTestName.c_str()), 0);
    EXPECT_EQ(context, nullptr);
    index = ((index + 1) % kNumSources);
  };

  EXPECT_EQ(source_expired_fake.call_count, 0u);
  ProcessSourceDetector();
  EXPECT_EQ(source_expired_fake.call_count, kNumSources);
}

TEST_F(TestSourceDetectorState, SourceExpiredWorksOneAtATime)
{
  static constexpr unsigned int kNumSources = 3u;
  static etcpal::Uuid test_cids[kNumSources];

  std::vector<uint16_t> universe_list = {1u, 2u, 3u};

  for (unsigned int i = 0u; i < kNumSources; ++i)
  {
    test_cids[i] = etcpal::Uuid::V4();
    ProcessUniverseDiscoveryPages(test_cids[i], universe_list);
    etcpal_getms_fake.return_val += 200u;
  }

  etcpal_getms_fake.return_val += ((SACN_UNIVERSE_DISCOVERY_INTERVAL * 2u) - (200u * kNumSources));

  static unsigned int index = 0u;
  source_expired_fake.custom_fake = [](sacn_remote_source_t, const EtcPalUuid* cid, const char* name, void* context) {
    EXPECT_EQ(ETCPAL_UUID_CMP(cid, &test_cids[index].get()), 0);
    EXPECT_EQ(strcmp(name, kTestName.c_str()), 0);
    EXPECT_EQ(context, nullptr);
    index = ((index + 1) % kNumSources);
  };

  for (unsigned int i = 0u; i < kNumSources; ++i)
  {
    etcpal_getms_fake.return_val += 200u;
    EXPECT_EQ(source_expired_fake.call_count, i);
    ProcessSourceDetector();
    EXPECT_EQ(source_expired_fake.call_count, i + 1u);
  }
}

TEST_F(TestSourceDetectorState, SourceLimitExceededWorks)
{
  std::vector<uint16_t> universe_list = {1u, 2u, 3u};

  for (int i = 0; i < kTestMaxSources; ++i)
  {
    ProcessUniverseDiscoveryPages(etcpal::Uuid::V4(), universe_list);
    etcpal_getms_fake.return_val += 200u;
  }

  EXPECT_EQ(limit_exceeded_fake.call_count, 0u);
  ProcessUniverseDiscoveryPages(etcpal::Uuid::V4(), universe_list);
  EXPECT_EQ(limit_exceeded_fake.call_count, 1u);
  ProcessUniverseDiscoveryPages(etcpal::Uuid::V4(), universe_list);
  EXPECT_EQ(limit_exceeded_fake.call_count, 1u);
  ProcessUniverseDiscoveryPages(etcpal::Uuid::V4(), universe_list);
  EXPECT_EQ(limit_exceeded_fake.call_count, 1u);

  // Now remove a source to end suppression.
  EXPECT_EQ(source_expired_fake.call_count, 0u);
  etcpal_getms_fake.return_val += ((SACN_UNIVERSE_DISCOVERY_INTERVAL * 2u) - (200u * (kTestMaxSources - 1u)));
  ProcessSourceDetector();
  EXPECT_EQ(source_expired_fake.call_count, 1u);

  ProcessUniverseDiscoveryPages(etcpal::Uuid::V4(), universe_list);
  EXPECT_EQ(limit_exceeded_fake.call_count, 1u);
  ProcessUniverseDiscoveryPages(etcpal::Uuid::V4(), universe_list);
  EXPECT_EQ(limit_exceeded_fake.call_count, 2u);
}

TEST_F(TestSourceDetectorState, UniverseLimitExceededWorks)
{
  static std::vector<uint16_t> universe_list;
  universe_list.clear();
  for (uint16_t universe = 1u; universe <= kTestMaxUniverses; ++universe)
    universe_list.push_back(universe);

  static etcpal::Uuid test_cid = etcpal::Uuid::V4();

  source_updated_fake.custom_fake = [](sacn_remote_source_t, const EtcPalUuid* cid, const char* name,
                                       const uint16_t* sourced_universes, size_t num_sourced_universes, void* context) {
    EXPECT_EQ(ETCPAL_UUID_CMP(cid, &test_cid.get()), 0);
    EXPECT_EQ(strcmp(name, kTestName.c_str()), 0);

    for (size_t i = 0u; i < num_sourced_universes; ++i)
      EXPECT_EQ(sourced_universes[i], universe_list[i]);

    if (universe_list.size() < kTestMaxUniverses)
      EXPECT_EQ(num_sourced_universes, universe_list.size());
    else
      EXPECT_EQ(num_sourced_universes, kTestMaxUniverses);

    EXPECT_EQ(context, nullptr);
  };

  EXPECT_EQ(source_updated_fake.call_count, 0u);
  EXPECT_EQ(limit_exceeded_fake.call_count, 0u);
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
  EXPECT_EQ(limit_exceeded_fake.call_count, 0u);
  universe_list.push_back(kTestMaxUniverses + 1u);
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 2u);
  EXPECT_EQ(limit_exceeded_fake.call_count, 1u);
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 3u);
  EXPECT_EQ(limit_exceeded_fake.call_count, 1u);
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 4u);
  EXPECT_EQ(limit_exceeded_fake.call_count, 1u);

  // Now end suppression by removing the last known universe.
  universe_list.pop_back();
  universe_list.pop_back();
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 5u);
  EXPECT_EQ(limit_exceeded_fake.call_count, 1u);
  universe_list.push_back(kTestMaxUniverses);
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 6u);
  EXPECT_EQ(limit_exceeded_fake.call_count, 1u);
  universe_list.push_back(kTestMaxUniverses + 1u);
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 7u);
  EXPECT_EQ(limit_exceeded_fake.call_count, 2u);
}

TEST_F(TestSourceDetectorState, SourceUpdatedOnlyNotifiesOnChange)
{
  std::vector<uint16_t> universe_list;
  etcpal::Uuid test_cid = etcpal::Uuid::V4();
  for (uint16_t i = 0u; i < 2u; ++i)
  {
    for (uint16_t universe = (500u * i); universe < (500u * (i + 1u)); ++universe)
      universe_list.push_back(universe);

    EXPECT_EQ(source_updated_fake.call_count, i);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
  }

  for (uint16_t i = 2u; i < 1002u; ++i)
  {
    universe_list.push_back(998u + i);

    EXPECT_EQ(source_updated_fake.call_count, i);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
  }

  for (unsigned int i = 1002u; i < 3002u; ++i)
  {
    ++universe_list[3001u - i];

    EXPECT_EQ(source_updated_fake.call_count, i);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
  }

  for (unsigned int i = 3002u; i < 3004u; ++i)
  {
    for (int j = 0; j < 500; ++j)
      universe_list.pop_back();

    EXPECT_EQ(source_updated_fake.call_count, i);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
  }

  for (unsigned int i = 3004u; i < 4004u; ++i)
  {
    universe_list.pop_back();

    EXPECT_EQ(source_updated_fake.call_count, i);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
    ProcessUniverseDiscoveryPages(test_cid, universe_list);
    EXPECT_EQ(source_updated_fake.call_count, i + 1u);
  }
}

TEST_F(TestSourceDetectorState, SourceUpdatedWorksWithEmptyUniverseList)
{
  std::vector<uint16_t> universe_list;
  etcpal::Uuid test_cid = etcpal::Uuid::V4();

  source_updated_fake.custom_fake = [](sacn_remote_source_t, const EtcPalUuid*, const char*,
                                       const uint16_t* sourced_universes, size_t num_sourced_universes, void*) {
    EXPECT_EQ(sourced_universes, nullptr);
    EXPECT_EQ(num_sourced_universes, 0u);
  };

  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
  ProcessUniverseDiscoveryPages(test_cid, universe_list);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
}

TEST_F(TestSourceDetectorState, SourceUpdatedFiltersNonAscendingLists)
{
  etcpal::Uuid test_cid = etcpal::Uuid::V4();
  std::vector<uint16_t> descending_list = {5u, 4u, 3u, 2u, 1u};
  std::vector<uint16_t> ascending_list = {1u, 2u, 3u, 4u, 5u};
  std::vector<uint16_t> unordered_list = {3u, 5u, 4u, 1u, 2u};

  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPages(test_cid, descending_list);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPages(test_cid, ascending_list);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
  ProcessUniverseDiscoveryPages(test_cid, unordered_list);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
  ProcessUniverseDiscoveryPages(test_cid, ascending_list);
  EXPECT_EQ(source_updated_fake.call_count, 2u);
}
