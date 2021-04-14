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

FAKE_VOID_FUNC(source_updated, const EtcPalUuid*, const char*, const uint16_t*, size_t, void*);
FAKE_VOID_FUNC(source_expired, const EtcPalUuid*, const char*, void*);
FAKE_VOID_FUNC(limit_exceeded, void*);

static const EtcPalSockAddr kTestSourceAddr = {SACN_PORT, etcpal::IpAddr::FromString("10.101.1.1").get()};
const std::string kTestName = "Test Name";

#if SACN_DYNAMIC_MEM
constexpr int kTestMaxSources = 3;
constexpr int kTestMaxUniverses = 2000;
#else
constexpr int kTestMaxSources = SACN_SOURCE_DETECTOR_MAX_SOURCES;
constexpr int kTestMaxUniverses = SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE;
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
    int last_page = (complete_universe_list.size() / 512);

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

  SacnSourceDetector* detector_{nullptr};
};

TEST_F(TestSourceDetectorState, SourceUpdatedWorks)
{
  static std::vector<uint16_t> universe_list_1, universe_list_2;
  static etcpal::Uuid test_cid_1;
  static constexpr uint16_t kNumUniverses = 1000u;

  universe_list_1.clear();
  universe_list_2.clear();

  for (uint16_t universe = 0u; universe < kNumUniverses; ++universe)
    universe_list_1.push_back(universe);
  for (uint16_t universe = kNumUniverses; universe < (kNumUniverses * 2u); ++universe)
    universe_list_2.push_back(universe);

  test_cid_1 = etcpal::Uuid::V4();

  source_updated_fake.custom_fake = [](const EtcPalUuid* cid, const char* name, const uint16_t* sourced_universes,
                                       size_t num_sourced_universes, void* context) {
    EXPECT_EQ(ETCPAL_UUID_CMP(cid, &test_cid_1.get()), 0);
    EXPECT_EQ(strcmp(name, kTestName.c_str()), 0);

    for (size_t i = 0u; i < num_sourced_universes; ++i)
      EXPECT_EQ(sourced_universes[i], universe_list_1[i]);

    EXPECT_EQ(num_sourced_universes, kNumUniverses);
    EXPECT_EQ(context, nullptr);
  };

  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid_1, universe_list_1, 0u);
  EXPECT_EQ(source_updated_fake.call_count, 0u);
  ProcessUniverseDiscoveryPage(test_cid_1, universe_list_1, 1u);
  EXPECT_EQ(source_updated_fake.call_count, 1u);

  source_updated_fake.custom_fake = [](const EtcPalUuid* cid, const char* name, const uint16_t* sourced_universes,
                                       size_t num_sourced_universes, void* context) {
    EXPECT_EQ(ETCPAL_UUID_CMP(cid, &test_cid_1.get()), 0);
    EXPECT_EQ(strcmp(name, kTestName.c_str()), 0);

    for (size_t i = 0u; i < num_sourced_universes; ++i)
      EXPECT_EQ(sourced_universes[i], universe_list_2[i]);

    EXPECT_EQ(num_sourced_universes, kNumUniverses);
    EXPECT_EQ(context, nullptr);
  };

  ProcessUniverseDiscoveryPage(test_cid_1, universe_list_2, 1u);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
  ProcessUniverseDiscoveryPage(test_cid_1, universe_list_2, 0u);
  EXPECT_EQ(source_updated_fake.call_count, 1u);
  ProcessUniverseDiscoveryPage(test_cid_1, universe_list_2, 1u);
  EXPECT_EQ(source_updated_fake.call_count, 2u);
}
