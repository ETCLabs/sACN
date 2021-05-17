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

#include "sacn/private/source_state.h"

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
#define TestSourceState TestSourceStateDynamic
#else
#define TestSourceState TestSourceStateStatic
#endif

#define IS_UNIVERSE_DISCOVERY(send_buf)                                                         \
  ((etcpal_unpack_u32b(&send_buf[SACN_ROOT_VECTOR_OFFSET]) == ACN_VECTOR_ROOT_E131_EXTENDED) && \
   (etcpal_unpack_u32b(&send_buf[SACN_FRAMING_VECTOR_OFFSET]) == VECTOR_E131_EXTENDED_DISCOVERY))
#define IS_UNIVERSE_DATA(send_buf)                                                          \
  ((etcpal_unpack_u32b(&send_buf[SACN_ROOT_VECTOR_OFFSET]) == ACN_VECTOR_ROOT_E131_DATA) && \
   (etcpal_unpack_u32b(&send_buf[SACN_FRAMING_VECTOR_OFFSET]) == VECTOR_E131_DATA_PACKET))
#define VERIFY_LOCKING(function_call)                                  \
  do                                                                   \
  {                                                                    \
    unsigned int previous_lock_count = sacn_lock_fake.call_count;      \
    function_call;                                                     \
    EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);         \
    EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count); \
  } while (0)
#define VERIFY_LOCKING_AND_RETURN_VALUE(function_call, expected_return_value) \
  do                                                                          \
  {                                                                           \
    unsigned int previous_lock_count = sacn_lock_fake.call_count;             \
    EXPECT_EQ(function_call, expected_return_value);                          \
    EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);                \
    EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);        \
  } while (0)

static const SacnSourceConfig kTestSourceConfig = {
    etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22").get(),
    "Test Source",
    SACN_SOURCE_INFINITE_UNIVERSES,
    false,
    kSacnIpV4AndIpV6,
    SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT};
static constexpr SacnSourceUniverseConfig kTestUniverseConfig = {1, 100, false, false, NULL, 0, 0};

static std::vector<SacnMcastInterface> kTestNetints = {{{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}};

static const std::vector<uint8_t> kTestBuffer = {
    0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu,
};
static const std::vector<uint8_t> kTestBuffer2 = {
    0x0Du, 0x0Eu, 0x0Fu, 0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u, 0x19u, 0x1Au,
};

static const std::vector<EtcPalIpAddr> kTestRemoteAddrs = {
    etcpal::IpAddr::FromString("10.101.1.1").get(), etcpal::IpAddr::FromString("10.101.1.2").get(),
    etcpal::IpAddr::FromString("10.101.1.3").get(), etcpal::IpAddr::FromString("10.101.1.4").get()};

static constexpr uint32_t kTestGetMsValue = 1234567u;
static constexpr uint32_t kTestGetMsValue2 = 2345678u;
static constexpr uint8_t kTestPriority = 123u;
static const std::string kTestName = "Test Name";

// Some of the tests use these variables to communicate with their custom_fake lambdas.
static unsigned int num_universe_discovery_sends = 0u;
static unsigned int num_universe_data_sends = 0u;
static unsigned int num_level_multicast_sends = 0u;
static unsigned int num_pap_multicast_sends = 0u;
static unsigned int num_level_unicast_sends = 0u;
static unsigned int num_pap_unicast_sends = 0u;
static unsigned int num_invalid_sends = 0u;
static int current_test_iteration = 0;
static int current_remote_addr_index = 0;
static int current_universe = 0;
static int current_netint_index = 0;

class TestSourceState : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_sockets_reset_all_fakes();

    sacn_initialize_source_netints_fake.custom_fake = [](SacnInternalNetintArray* source_netints,
                                                         SacnMcastInterface* app_netints, size_t num_app_netints) {
#if SACN_DYNAMIC_MEM
      source_netints->netints = (EtcPalMcastNetintId*)calloc(num_app_netints, sizeof(EtcPalMcastNetintId));
#endif
      source_netints->num_netints = num_app_netints;

      for (size_t i = 0; i < num_app_netints; ++i)
      {
        source_netints->netints[i] = app_netints[i].iface;
        app_netints[i].status = kEtcPalErrOk;
      }

      return kEtcPalErrOk;
    };

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_source_state_init(), kEtcPalErrOk);

    num_universe_data_sends = 0u;
    num_universe_discovery_sends = 0u;
    num_level_multicast_sends = 0u;
    num_pap_multicast_sends = 0u;
    num_level_unicast_sends = 0u;
    num_pap_unicast_sends = 0u;
    num_invalid_sends = 0u;
  }

  void TearDown() override
  {
    next_source_handle_ = 0;
    sacn_source_state_deinit();
    sacn_mem_deinit();
  }

  sacn_source_t AddSource(const SacnSourceConfig& config)
  {
    SacnSource* tmp = nullptr;
    EXPECT_EQ(add_sacn_source(next_source_handle_++, &config, &tmp), kEtcPalErrOk);
    return (next_source_handle_ - 1);
  }

  SacnSource* GetSource(sacn_source_t handle)
  {
    SacnSource* state = nullptr;
    lookup_source(handle, &state);
    return state;
  }

  uint16_t AddUniverse(sacn_source_t source, const SacnSourceUniverseConfig& config,
                       std::vector<SacnMcastInterface>& netints = kTestNetints)
  {
    SacnSourceUniverse* tmp = nullptr;
    EXPECT_EQ(add_sacn_source_universe(GetSource(source), &config, netints.data(), netints.size(), &tmp), kEtcPalErrOk);

    for (size_t i = 0; i < netints.size(); ++i)
      EXPECT_EQ(add_sacn_source_netint(GetSource(source), &netints[i].iface), kEtcPalErrOk);

    return config.universe;
  }

  uint16_t AddUniverse(sacn_source_t source, const SacnSourceUniverseConfig& config, SacnMcastInterface& netint)
  {
    SacnSourceUniverse* tmp = nullptr;
    EXPECT_EQ(add_sacn_source_universe(GetSource(source), &config, &netint, 1u, &tmp), kEtcPalErrOk);
    EXPECT_EQ(add_sacn_source_netint(GetSource(source), &netint.iface), kEtcPalErrOk);

    return config.universe;
  }

  SacnSourceUniverse* GetUniverse(sacn_source_t source, uint16_t universe)
  {
    SacnSource* source_state = nullptr;
    SacnSourceUniverse* universe_state = nullptr;
    lookup_source_and_universe(source, universe, &source_state, &universe_state);
    return universe_state;
  }

  void InitTestData(sacn_source_t source, uint16_t universe, const std::vector<uint8_t>& levels,
                    const std::vector<uint8_t>& paps = std::vector<uint8_t>())
  {
    update_levels_and_or_paps(GetSource(source), GetUniverse(source, universe),
                              levels.empty() ? nullptr : levels.data(), levels.size(),
                              paps.empty() ? nullptr : paps.data(), paps.size(), kDisableForceSync);
  }

  void AddUniverseForUniverseDiscovery(sacn_source_t source_handle, SacnSourceUniverseConfig& universe_config,
                                       std::vector<SacnMcastInterface>& netints = kTestNetints)
  {
    AddUniverse(source_handle, universe_config, netints);
    InitTestData(source_handle, universe_config.universe, kTestBuffer);
    ++universe_config.universe;
  }

  void AddUniverseForUniverseDiscovery(sacn_source_t source_handle, SacnSourceUniverseConfig& universe_config,
                                       SacnMcastInterface& netint)
  {
    AddUniverse(source_handle, universe_config, netint);
    InitTestData(source_handle, universe_config.universe, kTestBuffer);
    ++universe_config.universe;
  }

  void AddTestUnicastDests(sacn_source_t source, uint16_t universe)
  {
    SacnUnicastDestination* tmp = nullptr;
    for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
      EXPECT_EQ(add_sacn_unicast_dest(GetUniverse(source, universe), &kTestRemoteAddrs[i], &tmp), kEtcPalErrOk);
  }

  void TestLevelPapTransmission(int keep_alive_interval)
  {
    etcpal_getms_fake.return_val = 0u;

    sacn_send_multicast_fake.custom_fake = [](uint16_t universe_id, sacn_ip_support_t ip_supported,
                                              const uint8_t* send_buf, const EtcPalMcastNetintId* netint) {
      if (IS_UNIVERSE_DATA(send_buf))
      {
        EXPECT_EQ(universe_id, kTestUniverseConfig.universe);
        EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);

        if (memcmp(&send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer.data(), kTestBuffer.size()) == 0)
        {
          ++num_level_multicast_sends;
          EXPECT_EQ(num_level_multicast_sends, num_pap_multicast_sends + current_netint_index + 1);
        }
        else if (memcmp(&send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer2.data(), kTestBuffer2.size()) == 0)
        {
          ++num_pap_multicast_sends;
          EXPECT_EQ(num_pap_multicast_sends,
                    (num_level_multicast_sends - kTestNetints.size()) + current_netint_index + 1);
        }
        else
        {
          ++num_invalid_sends;
        }

        EXPECT_EQ(kTestNetints[current_netint_index].iface.index, netint->index);
        EXPECT_EQ(kTestNetints[current_netint_index].iface.ip_type, netint->ip_type);

        current_netint_index = (current_netint_index + 1) % kTestNetints.size();
      }
    };
    sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                                            const EtcPalIpAddr* dest_addr) {
      if (IS_UNIVERSE_DATA(send_buf))
      {
        EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);

        if (memcmp(&send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer.data(), kTestBuffer.size()) == 0)
        {
          ++num_level_unicast_sends;
          EXPECT_EQ(num_level_unicast_sends, num_pap_unicast_sends + current_remote_addr_index + 1);
        }
        else if (memcmp(&send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer2.data(), kTestBuffer2.size()) == 0)
        {
          ++num_pap_unicast_sends;
          EXPECT_EQ(num_pap_unicast_sends,
                    (num_level_unicast_sends - kTestRemoteAddrs.size()) + current_remote_addr_index + 1);
        }
        else
        {
          ++num_invalid_sends;
        }

        EXPECT_EQ(etcpal_ip_cmp(&kTestRemoteAddrs[current_remote_addr_index], dest_addr), 0);

        current_remote_addr_index = (current_remote_addr_index + 1) % kTestRemoteAddrs.size();
      }
    };

    SacnSourceConfig source_config = kTestSourceConfig;
    source_config.keep_alive_interval = keep_alive_interval;
    sacn_source_t source = AddSource(source_config);
    uint16_t universe = AddUniverse(source, kTestUniverseConfig);
    AddTestUnicastDests(source, universe);
    InitTestData(source, universe, kTestBuffer, kTestBuffer2);

    current_netint_index = 0;
    current_remote_addr_index = 0;

    for (int i = 0; i < 5; ++i)
    {
      EXPECT_EQ(GetUniverse(source, universe)->level_packets_sent_before_suppression, i);
      EXPECT_EQ(GetUniverse(source, universe)->pap_packets_sent_before_suppression, i);
      EXPECT_EQ(GetUniverse(source, universe)->seq_num, (uint8_t)(i * 2));
      VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
    }

    EXPECT_EQ(GetUniverse(source, universe)->level_packets_sent_before_suppression, 4);
    EXPECT_EQ(GetUniverse(source, universe)->pap_packets_sent_before_suppression, 4);
    EXPECT_EQ(GetUniverse(source, universe)->seq_num, 0x08u);

    EXPECT_EQ(num_level_multicast_sends, kTestNetints.size() * 4u);
    EXPECT_EQ(num_pap_multicast_sends, kTestNetints.size() * 4u);
    EXPECT_EQ(num_level_unicast_sends, kTestRemoteAddrs.size() * 4u);
    EXPECT_EQ(num_pap_unicast_sends, kTestRemoteAddrs.size() * 4u);

    num_level_multicast_sends = 0u;
    num_pap_multicast_sends = 0u;
    num_level_unicast_sends = 0u;
    num_pap_unicast_sends = 0u;

    for (unsigned int i = 1u; i <= 7u; ++i)
    {
      for (int j = 0; j <= 10; ++j)
      {
        etcpal_getms_fake.return_val += ((source_config.keep_alive_interval / 10) + 1);
        VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
      }

      EXPECT_EQ(GetUniverse(source, universe)->level_packets_sent_before_suppression, 4);
      EXPECT_EQ(GetUniverse(source, universe)->pap_packets_sent_before_suppression, 4);
      EXPECT_EQ(GetUniverse(source, universe)->seq_num, 0x08u + (0x02u * (uint8_t)i));

      EXPECT_EQ(num_level_multicast_sends, kTestNetints.size() * i);
      EXPECT_EQ(num_pap_multicast_sends, kTestNetints.size() * i);
      EXPECT_EQ(num_level_unicast_sends, kTestRemoteAddrs.size() * i);
      EXPECT_EQ(num_pap_unicast_sends, kTestRemoteAddrs.size() * i);
    }

    EXPECT_EQ(num_invalid_sends, 0u);
  }

  sacn_source_t next_source_handle_ = 0;
};

TEST_F(TestSourceState, DeinitJoinsInitializedThread)
{
  EXPECT_EQ(etcpal_thread_join_fake.call_count, 0u);

  initialize_source_thread();
  sacn_source_state_deinit();

  EXPECT_EQ(etcpal_thread_join_fake.call_count, 1u);
}

TEST_F(TestSourceState, DeinitDoesNotJoinUninitializedThread)
{
  EXPECT_EQ(etcpal_thread_join_fake.call_count, 0u);

  sacn_source_state_deinit();

  EXPECT_EQ(etcpal_thread_join_fake.call_count, 0u);
}

TEST_F(TestSourceState, DeinitDoesNotJoinFailedThread)
{
  etcpal_thread_create_fake.return_val = kEtcPalErrSys;

  EXPECT_EQ(etcpal_thread_join_fake.call_count, 0u);

  initialize_source_thread();
  sacn_source_state_deinit();

  EXPECT_EQ(etcpal_thread_join_fake.call_count, 0u);
}

TEST_F(TestSourceState, ProcessSourcesCountsSources)
{
  SacnSourceConfig config = kTestSourceConfig;

  config.manually_process_source = true;
  AddSource(config);
  AddSource(config);
  AddSource(config);
  int num_manual_sources = (int)get_num_sources();

  config.manually_process_source = false;
  AddSource(config);
  AddSource(config);
  int num_threaded_sources = ((int)get_num_sources() - num_manual_sources);

  VERIFY_LOCKING_AND_RETURN_VALUE(take_lock_and_process_sources(kProcessManualSources), num_manual_sources);
  VERIFY_LOCKING_AND_RETURN_VALUE(take_lock_and_process_sources(kProcessThreadedSources), num_threaded_sources);
}

TEST_F(TestSourceState, ProcessSourcesMarksTerminatingOnDeinit)
{
  SacnSourceConfig source_config = kTestSourceConfig;
  source_config.manually_process_source = true;
  sacn_source_t manual_source_1 = AddSource(source_config);
  sacn_source_t manual_source_2 = AddSource(source_config);
  source_config.manually_process_source = false;
  sacn_source_t threaded_source_1 = AddSource(source_config);
  sacn_source_t threaded_source_2 = AddSource(source_config);

  // Add universes with levels so sources don't get deleted right away, so terminating flag can be verified.
  AddUniverse(threaded_source_1, kTestUniverseConfig);
  AddUniverse(threaded_source_2, kTestUniverseConfig);

  InitTestData(threaded_source_1, kTestUniverseConfig.universe, kTestBuffer);
  InitTestData(threaded_source_2, kTestUniverseConfig.universe, kTestBuffer);

  EXPECT_EQ(initialize_source_thread(), kEtcPalErrOk);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessManualSources));
  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ((GetSource(manual_source_1))->terminating, false);
  EXPECT_EQ((GetSource(manual_source_2))->terminating, false);
  EXPECT_EQ((GetSource(threaded_source_1))->terminating, false);
  EXPECT_EQ((GetSource(threaded_source_2))->terminating, false);

  sacn_source_state_deinit();

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessManualSources));

  EXPECT_EQ((GetSource(manual_source_1))->terminating, false);
  EXPECT_EQ((GetSource(manual_source_2))->terminating, false);
  EXPECT_EQ((GetSource(threaded_source_1))->terminating, false);
  EXPECT_EQ((GetSource(threaded_source_2))->terminating, false);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ((GetSource(manual_source_1))->terminating, false);
  EXPECT_EQ((GetSource(manual_source_2))->terminating, false);
  EXPECT_EQ((GetSource(threaded_source_1))->terminating, true);
  EXPECT_EQ((GetSource(threaded_source_2))->terminating, true);
}

TEST_F(TestSourceState, UniverseDiscoveryTimingIsCorrect)
{
  etcpal_getms_fake.return_val = 0u;

  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
      ++num_universe_discovery_sends;
  };

  sacn_source_t source_handle = AddSource(kTestSourceConfig);
  AddUniverse(source_handle, kTestUniverseConfig);
  InitTestData(source_handle, kTestUniverseConfig.universe, kTestBuffer);

  for (int i = 0; i < 10; ++i)
  {
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
    EXPECT_EQ(num_universe_discovery_sends, kTestNetints.size() * i);

    etcpal_getms_fake.return_val += SACN_UNIVERSE_DISCOVERY_INTERVAL;

    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
    EXPECT_EQ(num_universe_discovery_sends, kTestNetints.size() * i);

    ++etcpal_getms_fake.return_val;
  }
}

TEST_F(TestSourceState, SourceTerminatingStopsUniverseDiscovery)
{
  etcpal_getms_fake.return_val = 0u;

  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
      ++num_universe_discovery_sends;
  };

  sacn_source_t source_handle = AddSource(kTestSourceConfig);
  AddUniverse(source_handle, kTestUniverseConfig);
  InitTestData(source_handle, kTestUniverseConfig.universe, kTestBuffer);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  EXPECT_EQ(num_universe_discovery_sends, 0u);

  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  EXPECT_EQ(num_universe_discovery_sends, kTestNetints.size());

  set_source_terminating(GetSource(source_handle));
  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  EXPECT_EQ(num_universe_discovery_sends, kTestNetints.size());
  EXPECT_EQ(get_num_sources(), 1u);
}

TEST_F(TestSourceState, UniverseDiscoverySendsForEachPage)
{
  etcpal_getms_fake.return_val = 0u;

  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
      ++num_universe_discovery_sends;
  };

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int num_pages = 1; num_pages <= 4; ++num_pages)
  {
    for (int i = 0; i < SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE; ++i)
      AddUniverseForUniverseDiscovery(source_handle, universe_config);

    etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);

    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
    EXPECT_EQ(num_universe_discovery_sends, num_pages * kTestNetints.size());

    num_universe_discovery_sends = 0u;
  }
}

TEST_F(TestSourceState, UniverseDiscoverySendsCorrectUniverseLists)
{
  ASSERT_EQ(SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE % 4, 0);

  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
    {
      int page = send_buf[SACN_UNIVERSE_DISCOVERY_PAGE_OFFSET];
      int last_page = send_buf[SACN_UNIVERSE_DISCOVERY_LAST_PAGE_OFFSET];
      int max_universes_per_page = SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE;
      int expected_num_universes =
          (page < last_page)
              ? max_universes_per_page
              : ((((current_test_iteration * (max_universes_per_page / 4)) - 1) % max_universes_per_page) + 1);
      int actual_num_universes = (ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE])) + ACN_UDP_PREAMBLE_SIZE -
                                  SACN_UNIVERSE_DISCOVERY_HEADER_SIZE) /
                                 2;

      EXPECT_EQ(actual_num_universes, expected_num_universes);

      for (int i = 0; i < expected_num_universes; ++i)
      {
        int expected_universe = i + 1 + (page * max_universes_per_page);
        int actual_universe = etcpal_unpack_u16b(&send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (i * 2)]);
        EXPECT_EQ(actual_universe, expected_universe);
      }
    }
  };

  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int i = 0; i < 10; ++i)
  {
    current_test_iteration = (i + 1);

    for (int j = 0; j < (SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE / 4); ++j)
      AddUniverseForUniverseDiscovery(source_handle, universe_config);

    etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  }
}

TEST_F(TestSourceState, UniverseDiscoverySendsCorrectPageNumbers)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
    {
      EXPECT_EQ(send_buf[SACN_UNIVERSE_DISCOVERY_PAGE_OFFSET], num_universe_discovery_sends / kTestNetints.size());
      ++num_universe_discovery_sends;
    }
  };

  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int i = 0; i < SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE * 4; ++i)
    AddUniverseForUniverseDiscovery(source_handle, universe_config);

  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
}

TEST_F(TestSourceState, UniverseDiscoverySendsCorrectLastPage)
{
  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;

  for (int i = 0; i < 4; ++i)
  {
    current_test_iteration = i;

    sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                              const EtcPalMcastNetintId*) {
      if (IS_UNIVERSE_DISCOVERY(send_buf))
      {
        EXPECT_EQ(send_buf[SACN_UNIVERSE_DISCOVERY_LAST_PAGE_OFFSET], current_test_iteration);
      }
    };

    for (int j = 0; j < SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE; ++j)
      AddUniverseForUniverseDiscovery(source_handle, universe_config);

    etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  }
}

TEST_F(TestSourceState, UniverseDiscoverySendsCorrectSequenceNumber)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
    {
      EXPECT_EQ(send_buf[SACN_SEQ_OFFSET], num_universe_discovery_sends / kTestNetints.size());
      ++num_universe_discovery_sends;
    }
  };

  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int i = 0; i < 20; ++i)
  {
    for (int j = 0; j < 100; ++j)
      AddUniverseForUniverseDiscovery(source_handle, universe_config);

    etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  }
}

TEST_F(TestSourceState, UniverseDiscoveryUsesCorrectNetints)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId* netint) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
    {
      EXPECT_EQ(netint->ip_type, kTestNetints[num_universe_discovery_sends].iface.ip_type);
      EXPECT_EQ(netint->index, kTestNetints[num_universe_discovery_sends].iface.index);
      ++num_universe_discovery_sends;
    }
  };

  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (size_t i = 0u; i < kTestNetints.size(); ++i)
    AddUniverseForUniverseDiscovery(source_handle, universe_config, kTestNetints[i]);

  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(num_universe_discovery_sends, kTestNetints.size());
}

TEST_F(TestSourceState, UniverseDiscoveryExcludesUniversesWithoutData)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
    {
      int num_universes = (ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE])) + ACN_UDP_PREAMBLE_SIZE -
                           SACN_UNIVERSE_DISCOVERY_HEADER_SIZE) /
                          2;

      for (int i = 0; i < num_universes; ++i)
      {
        int universe = etcpal_unpack_u16b(&send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (i * 2)]);
        EXPECT_EQ(universe % 2, 0);
      }
    }
  };

  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int i = 0; i < 100; ++i)
  {
    AddUniverse(source_handle, universe_config);

    if (i % 2)
      InitTestData(source_handle, universe_config.universe, kTestBuffer);

    ++universe_config.universe;
  }

  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
}

TEST_F(TestSourceState, UniverseDiscoveryExcludesUnicastOnlyUniverses)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
    {
      int num_universes = (ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE])) + ACN_UDP_PREAMBLE_SIZE -
                           SACN_UNIVERSE_DISCOVERY_HEADER_SIZE) /
                          2;

      for (int i = 0; i < num_universes; ++i)
      {
        int universe = etcpal_unpack_u16b(&send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (i * 2)]);
        EXPECT_EQ(universe % 2, 1);
      }
    }
  };

  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int i = 0; i < 100; ++i)
  {
    universe_config.send_unicast_only = (i % 2);
    AddUniverseForUniverseDiscovery(source_handle, universe_config);
  }

  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
}

TEST_F(TestSourceState, RemovingUniversesUpdatesUniverseDiscovery)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DISCOVERY(send_buf))
    {
      int expected_num_universes = (10 - current_test_iteration);
      int actual_num_universes = (ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE])) + ACN_UDP_PREAMBLE_SIZE -
                                  SACN_UNIVERSE_DISCOVERY_HEADER_SIZE) /
                                 2;

      EXPECT_EQ(actual_num_universes, expected_num_universes);

      for (int i = 0; i < expected_num_universes; ++i)
      {
        int expected_universe = i + 1;
        int actual_universe = etcpal_unpack_u16b(&send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (i * 2)]);
        EXPECT_EQ(actual_universe, expected_universe);
      }

      ++num_universe_discovery_sends;
    }
  };

  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;

  for (int i = 0; i < 10; ++i)
    AddUniverseForUniverseDiscovery(source_handle, universe_config);

  for (current_test_iteration = 0; current_test_iteration < 10; ++current_test_iteration)
  {
    set_universe_terminating(GetUniverse(source_handle, (uint16_t)(10u - current_test_iteration)), kTerminateAndRemove);

    for (int i = 0; i < 3; ++i)
    {
      etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
      VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
    }

    EXPECT_EQ(num_universe_discovery_sends, kTestNetints.size() * 3u * (current_test_iteration + 1u));
  }
}

TEST_F(TestSourceState, UnicastDestsWithDataTerminateAndRemove)
{
  sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                                          const EtcPalIpAddr* dest_addr) {
    EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);
    EXPECT_NE(TERMINATED_OPT_SET(send_buf), 0x00u);
    EXPECT_EQ(etcpal_ip_cmp(dest_addr, &kTestRemoteAddrs[current_remote_addr_index]), 0);

    --current_remote_addr_index;
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, kTestUniverseConfig.universe, kTestBuffer);
  AddTestUnicastDests(source, kTestUniverseConfig.universe);

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
  {
    set_unicast_dest_terminating(&GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[i],
                                 kTerminateAndRemove);
  }

  for (int i = 0; i < 3; ++i)
  {
    uint8_t old_seq_num = GetUniverse(source, kTestUniverseConfig.universe)->seq_num;

    current_remote_addr_index = ((int)kTestRemoteAddrs.size() - 1);
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

    for (size_t j = 0u; j < kTestRemoteAddrs.size(); ++j)
      EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[j].num_terminations_sent, i + 1);

    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->num_unicast_dests,
              (i < 2) ? kTestRemoteAddrs.size() : 0u);
    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->seq_num - old_seq_num,
              (uint8_t)(kTestRemoteAddrs.size() + 1u));  // One sequence number for each unicast termination packet +
                                                         // one more for non-unicast, non-termination data.
    EXPECT_EQ(TERMINATED_OPT_SET(GetUniverse(source, kTestUniverseConfig.universe)->level_send_buf), 0x00u);
  }

  EXPECT_EQ(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size() * 3u);
}

TEST_F(TestSourceState, UnicastDestsWithDataTerminateWithoutRemoving)
{
  static int iteration = 0;
  static bool terminations_all_sent = false;
  sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                                          const EtcPalIpAddr* dest_addr) {
    EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);

    if (terminations_all_sent)
    {
      EXPECT_EQ(TERMINATED_OPT_SET(send_buf), 0x00u);
    }
    else if (current_remote_addr_index >= 0)
    {
      EXPECT_NE(TERMINATED_OPT_SET(send_buf), 0x00u);

      EXPECT_EQ(etcpal_ip_cmp(dest_addr, &kTestRemoteAddrs[current_remote_addr_index]), 0);
      --current_remote_addr_index;
    }

    if ((iteration == 2) && (current_remote_addr_index < 0))
      terminations_all_sent = true;
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, kTestUniverseConfig.universe, kTestBuffer);
  AddTestUnicastDests(source, kTestUniverseConfig.universe);

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
  {
    set_unicast_dest_terminating(&GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[i],
                                 kTerminateWithoutRemoving);
  }

  terminations_all_sent = false;
  for (iteration = 0; iteration < 2; ++iteration)
  {
    uint8_t old_seq_num = GetUniverse(source, kTestUniverseConfig.universe)->seq_num;

    current_remote_addr_index = ((int)kTestRemoteAddrs.size() - 1);
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

    for (size_t j = 0u; j < kTestRemoteAddrs.size(); ++j)
    {
      EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[j].num_terminations_sent,
                iteration + 1);
      EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[j].termination_state,
                kTerminatingWithoutRemoving);
    }

    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->num_unicast_dests, kTestRemoteAddrs.size());
    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->seq_num - old_seq_num,
              (uint8_t)(kTestRemoteAddrs.size() + 1u));  // One sequence number for each unicast termination packet +
                                                         // one more for non-unicast, non-termination data.
    EXPECT_EQ(TERMINATED_OPT_SET(GetUniverse(source, kTestUniverseConfig.universe)->level_send_buf), 0x00u);
  }

  iteration = 2;

  current_remote_addr_index = ((int)kTestRemoteAddrs.size() - 1);
  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  for (size_t j = 0u; j < kTestRemoteAddrs.size(); ++j)
  {
    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[j].num_terminations_sent, 0);
    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[j].termination_state, kNotTerminating);
  }

  EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->num_unicast_dests, kTestRemoteAddrs.size());
  EXPECT_EQ(TERMINATED_OPT_SET(GetUniverse(source, kTestUniverseConfig.universe)->level_send_buf), 0x00u);

  EXPECT_GT(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size());
}

TEST_F(TestSourceState, UnicastDestsWithoutDataTerminateAndRemove)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  AddUniverse(source, kTestUniverseConfig);
  AddTestUnicastDests(source, kTestUniverseConfig.universe);

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
  {
    set_unicast_dest_terminating(&GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[i],
                                 kTerminateAndRemove);
  }

  uint8_t old_seq_num = GetUniverse(source, kTestUniverseConfig.universe)->seq_num;

  EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->num_unicast_dests, kTestRemoteAddrs.size());

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->num_unicast_dests, 0u);
  EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->seq_num - old_seq_num, (uint8_t)0u);  // No data to send.

  EXPECT_EQ(sacn_send_unicast_fake.call_count, 0u);
}

TEST_F(TestSourceState, UnicastDestsWithoutDataTerminateWithoutRemoving)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  AddUniverse(source, kTestUniverseConfig);
  AddTestUnicastDests(source, kTestUniverseConfig.universe);

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
  {
    set_unicast_dest_terminating(&GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[i],
                                 kTerminateWithoutRemoving);
  }

  uint8_t old_seq_num = GetUniverse(source, kTestUniverseConfig.universe)->seq_num;

  EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->num_unicast_dests, kTestRemoteAddrs.size());

  for (size_t j = 0u; j < kTestRemoteAddrs.size(); ++j)
  {
    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[j].termination_state,
              kTerminatingWithoutRemoving);
  }

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->num_unicast_dests, kTestRemoteAddrs.size());

  for (size_t j = 0u; j < kTestRemoteAddrs.size(); ++j)
  {
    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[j].num_terminations_sent, 0);
    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[j].termination_state, kNotTerminating);
  }

  EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->seq_num - old_seq_num, (uint8_t)0u);  // No data to send.

  EXPECT_EQ(sacn_send_unicast_fake.call_count, 0u);
}

TEST_F(TestSourceState, UniversesWithDataTerminateAndRemove)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t universe_id, sacn_ip_support_t ip_supported,
                                            const uint8_t* send_buf, const EtcPalMcastNetintId* netint) {
    if (IS_UNIVERSE_DATA(send_buf))
    {
      EXPECT_EQ(universe_id, current_universe);
      EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);
      EXPECT_NE(TERMINATED_OPT_SET(send_buf), 0x00u);
      EXPECT_EQ(netint->ip_type, kTestNetints[current_netint_index].iface.ip_type);
      EXPECT_EQ(netint->index, kTestNetints[current_netint_index].iface.index);

      current_netint_index = (current_netint_index + 1) % kTestNetints.size();

      if (current_netint_index == 0)
        --current_universe;

      ++num_universe_data_sends;
    }
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (universe_config.universe = 1; universe_config.universe <= 10u; ++universe_config.universe)
  {
    AddUniverse(source, universe_config);
    AddTestUnicastDests(source, universe_config.universe);
    InitTestData(source, universe_config.universe, kTestBuffer);
    set_universe_terminating(GetUniverse(source, universe_config.universe), kTerminateAndRemove);
  }

  for (int i = 0; i < 3; ++i)
  {
    uint8_t old_seq_num[10];
    for (uint16_t j = 0; j < 10u; ++j)
      old_seq_num[j] = GetUniverse(source, j + 1u)->seq_num;

    current_universe = 10;
    current_netint_index = 0;
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

    if (i < 2)
    {
      for (uint16_t j = 0u; j < 10u; ++j)
      {
        EXPECT_EQ(GetUniverse(source, j + 1u)->num_terminations_sent, i + 1);
        EXPECT_EQ(GetUniverse(source, j + 1u)->seq_num - old_seq_num[j], (uint8_t)(kTestRemoteAddrs.size() + 1u));
        EXPECT_EQ(TERMINATED_OPT_SET(GetUniverse(source, j + 1u)->level_send_buf), 0x00u);
      }

      EXPECT_EQ(GetSource(source)->num_universes, 10u);
    }
    else
    {
      EXPECT_EQ(GetSource(source)->num_universes, 0u);
    }
  }

  EXPECT_EQ(num_universe_data_sends, kTestNetints.size() * 30u);
}

TEST_F(TestSourceState, UniversesWithDataTerminateWithoutRemoving)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t universe_id, sacn_ip_support_t ip_supported,
                                            const uint8_t* send_buf, const EtcPalMcastNetintId* netint) {
    if (IS_UNIVERSE_DATA(send_buf))
    {
      EXPECT_EQ(universe_id, current_universe);
      EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);
      EXPECT_NE(TERMINATED_OPT_SET(send_buf), 0x00u);
      EXPECT_EQ(netint->ip_type, kTestNetints[current_netint_index].iface.ip_type);
      EXPECT_EQ(netint->index, kTestNetints[current_netint_index].iface.index);

      current_netint_index = (current_netint_index + 1) % kTestNetints.size();

      if (current_netint_index == 0)
        --current_universe;

      ++num_universe_data_sends;
    }
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (universe_config.universe = 1; universe_config.universe <= 10u; ++universe_config.universe)
  {
    AddUniverse(source, universe_config);
    AddTestUnicastDests(source, universe_config.universe);
    InitTestData(source, universe_config.universe, kTestBuffer);
    set_universe_terminating(GetUniverse(source, universe_config.universe), kTerminateWithoutRemoving);
  }

  for (int i = 0; i < 3; ++i)
  {
    uint8_t old_seq_num[10];
    for (uint16_t j = 0; j < 10u; ++j)
      old_seq_num[j] = GetUniverse(source, j + 1u)->seq_num;

    current_universe = 10;
    current_netint_index = 0;
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

    if (i < 2)
    {
      for (uint16_t j = 0u; j < 10u; ++j)
      {
        EXPECT_EQ(GetUniverse(source, j + 1u)->num_terminations_sent, i + 1);
        EXPECT_EQ(GetUniverse(source, j + 1u)->seq_num - old_seq_num[j], (uint8_t)(kTestRemoteAddrs.size() + 1u));
        EXPECT_EQ(TERMINATED_OPT_SET(GetUniverse(source, j + 1u)->level_send_buf), 0x00u);
      }
    }

    EXPECT_EQ(GetSource(source)->num_universes, 10u);
  }

  EXPECT_EQ(num_universe_data_sends, kTestNetints.size() * 30u);
}

TEST_F(TestSourceState, UniversesWithoutDataTerminateAndRemove)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DATA(send_buf))
      ++num_universe_data_sends;
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (universe_config.universe = 1; universe_config.universe <= 10u; ++universe_config.universe)
  {
    AddUniverse(source, universe_config);
    AddTestUnicastDests(source, universe_config.universe);
    set_universe_terminating(GetUniverse(source, universe_config.universe), kTerminateAndRemove);
  }

  EXPECT_EQ(GetSource(source)->num_universes, 10u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(GetSource(source)->num_universes, 0u);
  EXPECT_EQ(num_universe_data_sends, 0u);
}

TEST_F(TestSourceState, UniversesWithoutDataTerminateWithoutRemoving)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DATA(send_buf))
      ++num_universe_data_sends;
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (universe_config.universe = 1; universe_config.universe <= 10u; ++universe_config.universe)
  {
    AddUniverse(source, universe_config);
    AddTestUnicastDests(source, universe_config.universe);
    set_universe_terminating(GetUniverse(source, universe_config.universe), kTerminateWithoutRemoving);
  }

  EXPECT_EQ(GetSource(source)->num_universes, 10u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(GetSource(source)->num_universes, 10u);
  EXPECT_EQ(num_universe_data_sends, 0u);
}

TEST_F(TestSourceState, InterruptTerminatingWithoutRemovingWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  AddUniverse(source, kTestUniverseConfig);
  AddTestUnicastDests(source, kTestUniverseConfig.universe);
  InitTestData(source, kTestUniverseConfig.universe, kTestBuffer);

  set_universe_terminating(GetUniverse(source, kTestUniverseConfig.universe), kTerminateWithoutRemoving);

  // Allow one termination before interrupting
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    EXPECT_NE(TERMINATED_OPT_SET(send_buf), 0x00u);
  };

  sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t, const uint8_t* send_buf, const EtcPalIpAddr*) {
    EXPECT_NE(TERMINATED_OPT_SET(send_buf), 0x00u);
  };

  EXPECT_EQ(sacn_send_multicast_fake.call_count, 0u);
  EXPECT_EQ(sacn_send_unicast_fake.call_count, 0u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(sacn_send_multicast_fake.call_count, kTestNetints.size());
  EXPECT_EQ(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size());

  // Now interrupt
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    EXPECT_EQ(TERMINATED_OPT_SET(send_buf), 0x00u);
  };

  sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t, const uint8_t* send_buf, const EtcPalIpAddr*) {
    EXPECT_EQ(TERMINATED_OPT_SET(send_buf), 0x00u);
  };

  InitTestData(source, kTestUniverseConfig.universe, kTestBuffer2);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(sacn_send_multicast_fake.call_count, kTestNetints.size() * 2u);
  EXPECT_EQ(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size() * 2u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(sacn_send_multicast_fake.call_count, kTestNetints.size() * 3u);
  EXPECT_EQ(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size() * 3u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(sacn_send_multicast_fake.call_count, kTestNetints.size() * 4u);
  EXPECT_EQ(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size() * 4u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(sacn_send_multicast_fake.call_count, kTestNetints.size() * 5u);
  EXPECT_EQ(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size() * 5u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(sacn_send_multicast_fake.call_count, kTestNetints.size() * 5u);
  EXPECT_EQ(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size() * 5u);

  etcpal_getms_fake.return_val += (SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT + 1u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(sacn_send_multicast_fake.call_count, kTestNetints.size() * 6u);
  EXPECT_EQ(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size() * 6u);
}

TEST_F(TestSourceState, OnlyActiveUniverseRemovalsUpdateCounter)
{
  // Active universes are universes that should be included in universe discovery. Inactive universes should not. The
  // active universes counter should only decrement when an active universe is removed.
  sacn_source_t source = AddSource(kTestSourceConfig);
  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  uint16_t active_universe = AddUniverse(source, universe_config);
  InitTestData(source, active_universe, kTestBuffer);
  ++universe_config.universe;
  uint16_t inactive_universe_1 = AddUniverse(source, universe_config);
  ++universe_config.universe;
  universe_config.send_unicast_only = true;
  uint16_t inactive_universe_2 = AddUniverse(source, universe_config);
  InitTestData(source, active_universe, kTestBuffer);
  ++universe_config.universe;
  uint16_t inactive_universe_3 = AddUniverse(source, universe_config);

  size_t old_count = GetSource(source)->num_active_universes;

  set_universe_terminating(GetUniverse(source, inactive_universe_1), kTerminateAndRemove);
  for (int i = 0; i < 3; ++i)
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(GetSource(source)->num_active_universes, old_count);

  set_universe_terminating(GetUniverse(source, inactive_universe_2), kTerminateAndRemove);
  for (int i = 0; i < 3; ++i)
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(GetSource(source)->num_active_universes, old_count);

  set_universe_terminating(GetUniverse(source, inactive_universe_3), kTerminateAndRemove);
  for (int i = 0; i < 3; ++i)
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(GetSource(source)->num_active_universes, old_count);

  set_universe_terminating(GetUniverse(source, active_universe), kTerminateAndRemove);
  for (int i = 0; i < 3; ++i)
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(GetSource(source)->num_active_universes, old_count - 1u);
}

TEST_F(TestSourceState, UniverseRemovalUpdatesSourceNetints)
{
  sacn_source_t source = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (size_t num_netints = kTestNetints.size(); num_netints >= 1u; --num_netints)
  {
    SacnSourceUniverse* tmp = nullptr;
    EXPECT_EQ(add_sacn_source_universe(GetSource(source), &universe_config,
                                       &kTestNetints[kTestNetints.size() - num_netints], num_netints, &tmp),
              kEtcPalErrOk);

    for (size_t i = kTestNetints.size() - num_netints; i < kTestNetints.size(); ++i)
      EXPECT_EQ(add_sacn_source_netint(GetSource(source), &kTestNetints[i].iface), kEtcPalErrOk);

    ++universe_config.universe;
  }

  for (size_t i = 0u; i < kTestNetints.size(); ++i)
  {
    EXPECT_EQ(GetSource(source)->num_netints, kTestNetints.size() - i);
    for (size_t j = 0u; j < GetSource(source)->num_netints; ++j)
    {
      EXPECT_EQ(GetSource(source)->netints[j].id.ip_type, kTestNetints[j + i].iface.ip_type);
      EXPECT_EQ(GetSource(source)->netints[j].id.index, kTestNetints[j + i].iface.index);
      EXPECT_EQ(GetSource(source)->netints[j].num_refs, j + 1u);
    }

    set_universe_terminating(GetUniverse(source, (uint16_t)(i + 1u)), kTerminateAndRemove);
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  }

  EXPECT_EQ(GetSource(source)->num_netints, 0u);
}

TEST_F(TestSourceState, TransmitsLevelsAndPapsCorrectlyAtDefaultInterval)
{
  TestLevelPapTransmission(SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT);
}

TEST_F(TestSourceState, TransmitsLevelsAndPapsCorrectlyAtShortInterval)
{
  TestLevelPapTransmission(100);
}

TEST_F(TestSourceState, TransmitsLevelsAndPapsCorrectlyAtLongInterval)
{
  TestLevelPapTransmission(2000);
}

TEST_F(TestSourceState, SendUnicastOnlyWorks)
{
  etcpal_getms_fake.return_val = 0;

  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    if (IS_UNIVERSE_DATA(send_buf))
      ++num_universe_data_sends;
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  universe_config.send_unicast_only = true;
  uint16_t universe = AddUniverse(source, universe_config);
  AddTestUnicastDests(source, universe);
  InitTestData(source, universe, kTestBuffer, kTestBuffer2);

  for (int i = 0; i < 100; ++i)
  {
    etcpal_getms_fake.return_val += 100u;
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  }

  EXPECT_EQ(num_universe_data_sends, 0u);
  EXPECT_GT(sacn_send_unicast_fake.call_count, 0u);
}

TEST_F(TestSourceState, TerminatingUnicastDestsOnlySendTerminations)
{
  sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t, const uint8_t* send_buf, const EtcPalIpAddr* dest_addr) {
    if (etcpal_ip_cmp(dest_addr, &kTestRemoteAddrs[0]) == 0)
    {
      EXPECT_NE(TERMINATED_OPT_SET(send_buf), 0x00u);

      uint8_t start_code = send_buf[SACN_DATA_HEADER_SIZE - 1];
      EXPECT_EQ(start_code, 0x00u);
    }
    else
    {
      EXPECT_EQ(TERMINATED_OPT_SET(send_buf), 0x00u);
    }
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, kTestUniverseConfig.universe, kTestBuffer, kTestBuffer2);
  AddTestUnicastDests(source, kTestUniverseConfig.universe);

  set_unicast_dest_terminating(&GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[0],
                               kTerminateAndRemove);

  for (int i = 0; i < 100; ++i)
  {
    etcpal_getms_fake.return_val += 100u;
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  }
}

TEST_F(TestSourceState, PapNotTransmittedIfNotAdded)
{
  sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t, const uint8_t* send_buf, const EtcPalIpAddr*) {
    uint8_t start_code = send_buf[SACN_DATA_HEADER_SIZE - 1];
    EXPECT_EQ(start_code, 0x00u);
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, kTestUniverseConfig.universe, kTestBuffer);
  AddTestUnicastDests(source, kTestUniverseConfig.universe);

  for (int i = 0; i < 100; ++i)
  {
    etcpal_getms_fake.return_val += 100u;
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  }
}

TEST_F(TestSourceState, SourcesTerminateCorrectly)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (universe_config.universe = 1; universe_config.universe <= 10u; ++universe_config.universe)
  {
    AddUniverse(source, universe_config);
    AddTestUnicastDests(source, universe_config.universe);
    InitTestData(source, universe_config.universe, kTestBuffer);
  }

  set_source_terminating(GetSource(source));

  for (int i = 0; i < 3; ++i)
  {
    EXPECT_NE(GetSource(source), nullptr);
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  }

  EXPECT_EQ(GetSource(source), nullptr);
}

TEST_F(TestSourceState, InitializeSourceThreadWorks)
{
  etcpal_thread_create_fake.custom_fake = [](etcpal_thread_t* id, const EtcPalThreadParams* params,
                                             void (*thread_fn)(void*), void* thread_arg) {
    EXPECT_NE(id, nullptr);
    EXPECT_EQ(params->priority, (unsigned int)ETCPAL_THREAD_DEFAULT_PRIORITY);
    EXPECT_EQ(params->stack_size, (unsigned int)ETCPAL_THREAD_DEFAULT_STACK);
    EXPECT_EQ(params->platform_data, nullptr);
    EXPECT_NE(thread_fn, nullptr);
    EXPECT_EQ(thread_arg, nullptr);

    return kEtcPalErrOk;
  };

  EXPECT_EQ(etcpal_thread_create_fake.call_count, 0u);
  initialize_source_thread();
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 1u);
}

TEST_F(TestSourceState, GetNextSourceHandleWorks)
{
  sacn_source_t handle = get_next_source_handle();

  for (int i = 0; i < 10; ++i)
  {
    sacn_source_t prev_handle = handle;
    handle = get_next_source_handle();
    EXPECT_EQ(handle, prev_handle + 1);
  }
}

TEST_F(TestSourceState, UpdateLevelsAndPapsWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  lookup_source_and_universe(source, universe, &source_state, &universe_state);

  etcpal_getms_fake.return_val = kTestGetMsValue;

  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer2.data(),
                            kTestBuffer2.size(), kDisableForceSync);

  EXPECT_EQ(memcmp(&universe_state->level_send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer.data(), kTestBuffer.size()), 0);
  EXPECT_EQ(memcmp(&universe_state->pap_send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer2.data(), kTestBuffer2.size()), 0);
  EXPECT_EQ(universe_state->has_level_data, true);
  EXPECT_EQ(universe_state->has_pap_data, true);
  EXPECT_EQ(universe_state->level_keep_alive_timer.reset_time, kTestGetMsValue);
  EXPECT_EQ(universe_state->pap_keep_alive_timer.reset_time, kTestGetMsValue);
}

TEST_F(TestSourceState, UpdateOnlyLevelsWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  lookup_source_and_universe(source, universe, &source_state, &universe_state);

  etcpal_getms_fake.return_val = kTestGetMsValue;

  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), nullptr, 0u,
                            kDisableForceSync);

  EXPECT_EQ(memcmp(&universe_state->level_send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer.data(), kTestBuffer.size()), 0);
  EXPECT_EQ(universe_state->has_level_data, true);
  EXPECT_EQ(universe_state->has_pap_data, false);
  EXPECT_EQ(universe_state->level_keep_alive_timer.reset_time, kTestGetMsValue);
  EXPECT_NE(universe_state->pap_keep_alive_timer.reset_time, kTestGetMsValue);
}

TEST_F(TestSourceState, UpdateOnlyPapsWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  lookup_source_and_universe(source, universe, &source_state, &universe_state);

  etcpal_getms_fake.return_val = kTestGetMsValue;

  update_levels_and_or_paps(source_state, universe_state, nullptr, 0u, kTestBuffer2.data(), kTestBuffer2.size(),
                            kDisableForceSync);

  EXPECT_EQ(memcmp(&universe_state->pap_send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer2.data(), kTestBuffer2.size()), 0);
  EXPECT_EQ(universe_state->has_level_data, false);
  EXPECT_EQ(universe_state->has_pap_data, true);
  EXPECT_NE(universe_state->level_keep_alive_timer.reset_time, kTestGetMsValue);
  EXPECT_EQ(universe_state->pap_keep_alive_timer.reset_time, kTestGetMsValue);
}

TEST_F(TestSourceState, UpdateOnlyLevelsSavesPaps)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  lookup_source_and_universe(source, universe, &source_state, &universe_state);

  etcpal_getms_fake.return_val = kTestGetMsValue;
  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer.data(),
                            kTestBuffer.size(), kDisableForceSync);

  etcpal_getms_fake.return_val = kTestGetMsValue2;
  update_levels_and_or_paps(source_state, universe_state, kTestBuffer2.data(), kTestBuffer2.size(), nullptr, 0u,
                            kDisableForceSync);

  EXPECT_EQ(memcmp(&universe_state->pap_send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer.data(), kTestBuffer.size()), 0);
  EXPECT_EQ(universe_state->has_pap_data, true);
  EXPECT_EQ(universe_state->level_keep_alive_timer.reset_time, kTestGetMsValue2);
  EXPECT_EQ(universe_state->pap_keep_alive_timer.reset_time, kTestGetMsValue);
}

TEST_F(TestSourceState, UpdateOnlyPapsSavesLevels)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  lookup_source_and_universe(source, universe, &source_state, &universe_state);

  etcpal_getms_fake.return_val = kTestGetMsValue;
  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer.data(),
                            kTestBuffer.size(), kDisableForceSync);

  etcpal_getms_fake.return_val = kTestGetMsValue2;
  update_levels_and_or_paps(source_state, universe_state, nullptr, 0u, kTestBuffer2.data(), kTestBuffer2.size(),
                            kDisableForceSync);

  EXPECT_EQ(memcmp(&universe_state->level_send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer.data(), kTestBuffer.size()), 0);
  EXPECT_EQ(universe_state->has_level_data, true);
  EXPECT_EQ(universe_state->level_keep_alive_timer.reset_time, kTestGetMsValue);
  EXPECT_EQ(universe_state->pap_keep_alive_timer.reset_time, kTestGetMsValue2);
}

TEST_F(TestSourceState, LevelsZeroWhereverPapsAreZeroed)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  lookup_source_and_universe(source, universe, &source_state, &universe_state);

  etcpal_getms_fake.return_val = kTestGetMsValue;

  std::vector<uint8_t> pap_buffer = kTestBuffer2;

  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), pap_buffer.data(),
                            pap_buffer.size(), kDisableForceSync);

  for (size_t i = 0u; i < pap_buffer.size(); i += 2)
    pap_buffer[i] = 0u;

  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), pap_buffer.data(),
                            pap_buffer.size(), kDisableForceSync);

  for (size_t i = 0u; i < kTestBuffer.size(); ++i)
  {
    if (i % 2)
      EXPECT_GT(universe_state->level_send_buf[SACN_DATA_HEADER_SIZE + i], 0u);
    else
      EXPECT_EQ(universe_state->level_send_buf[SACN_DATA_HEADER_SIZE + i], 0u);
  }

  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), nullptr, 0u,
                            kDisableForceSync);

  for (size_t i = 0u; i < kTestBuffer.size(); ++i)
  {
    if (i % 2)
      EXPECT_GT(universe_state->level_send_buf[SACN_DATA_HEADER_SIZE + i], 0u);
    else
      EXPECT_EQ(universe_state->level_send_buf[SACN_DATA_HEADER_SIZE + i], 0u);
  }

  disable_pap_data(universe_state);
  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), nullptr, 0u,
                            kDisableForceSync);

  for (size_t i = 0u; i < kTestBuffer.size(); ++i)
    EXPECT_GT(universe_state->level_send_buf[SACN_DATA_HEADER_SIZE + i], 0u);

  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), &kTestPriority, 1u,
                            kDisableForceSync);

  for (size_t i = 0u; i < kTestBuffer.size(); ++i)
  {
    if (i == 0u)
      EXPECT_GT(universe_state->level_send_buf[SACN_DATA_HEADER_SIZE + i], 0u);
    else
      EXPECT_EQ(universe_state->level_send_buf[SACN_DATA_HEADER_SIZE + i], 0u);
  }
}

TEST_F(TestSourceState, UpdateLevelsIncrementsActiveUniversesCorrectly)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  lookup_source_and_universe(source, universe, &source_state, &universe_state);

  EXPECT_EQ(source_state->num_active_universes, 0u);
  update_levels_and_or_paps(source_state, universe_state, kTestBuffer.data(), kTestBuffer.size(), nullptr, 0u,
                            kDisableForceSync);
  EXPECT_EQ(source_state->num_active_universes, 1u);
  update_levels_and_or_paps(source_state, universe_state, kTestBuffer2.data(), kTestBuffer2.size(), nullptr, 0u,
                            kDisableForceSync);
  EXPECT_EQ(source_state->num_active_universes, 1u);

  SacnSourceUniverseConfig unicast_only_config = kTestUniverseConfig;
  ++unicast_only_config.universe;
  unicast_only_config.send_unicast_only = true;
  uint16_t unicast_only_universe = AddUniverse(source, unicast_only_config);
  SacnSourceUniverse* unicast_only_universe_state = nullptr;
  lookup_source_and_universe(source, unicast_only_universe, &source_state, &unicast_only_universe_state);

  update_levels_and_or_paps(source_state, unicast_only_universe_state, kTestBuffer.data(), kTestBuffer.size(), nullptr,
                            0u, kDisableForceSync);
  EXPECT_EQ(source_state->num_active_universes, 1u);
}

TEST_F(TestSourceState, IncrementSequenceNumberWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  lookup_source_and_universe(source, universe, &source_state, &universe_state);

  for (int i = 0; i < 255; ++i)
  {
    uint8_t old_seq_num = universe_state->seq_num;
    increment_sequence_number(universe_state);
    EXPECT_EQ(universe_state->seq_num, old_seq_num + 1u);
    EXPECT_EQ(universe_state->level_send_buf[SACN_SEQ_OFFSET], universe_state->seq_num);
    EXPECT_EQ(universe_state->pap_send_buf[SACN_SEQ_OFFSET], universe_state->seq_num);
  }

  EXPECT_EQ(universe_state->seq_num, 255u);
  increment_sequence_number(universe_state);
  EXPECT_EQ(universe_state->seq_num, 0u);
  EXPECT_EQ(universe_state->level_send_buf[SACN_SEQ_OFFSET], 0u);
  EXPECT_EQ(universe_state->pap_send_buf[SACN_SEQ_OFFSET], 0u);
}

TEST_F(TestSourceState, SendUniverseUnicastWorks)
{
  sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                                          const EtcPalIpAddr* dest_addr) {
    EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);
    EXPECT_EQ(memcmp(send_buf, kTestBuffer.data(), kTestBuffer.size()), 0);
    EXPECT_EQ(etcpal_ip_cmp(dest_addr, &kTestRemoteAddrs[current_remote_addr_index]), 0);
    ++current_remote_addr_index;
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  AddTestUnicastDests(source, universe);

  current_remote_addr_index = 0;
  send_universe_unicast(GetSource(source), GetUniverse(source, universe), kTestBuffer.data());
  EXPECT_EQ(sacn_send_unicast_fake.call_count, kTestRemoteAddrs.size());

  unsigned int num_terminating = 0u;
  for (size_t i = 1u; i < kTestRemoteAddrs.size(); i += 2u)
  {
    set_unicast_dest_terminating(&GetUniverse(source, universe)->unicast_dests[i], kTerminateAndRemove);
    ++num_terminating;
  }

  sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                                          const EtcPalIpAddr* dest_addr) {
    EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);
    EXPECT_EQ(memcmp(send_buf, kTestBuffer.data(), kTestBuffer.size()), 0);
    EXPECT_EQ(etcpal_ip_cmp(dest_addr, &kTestRemoteAddrs[current_remote_addr_index]), 0);
    current_remote_addr_index += 2;
  };

  current_remote_addr_index = 0;
  send_universe_unicast(GetSource(source), GetUniverse(source, universe), kTestBuffer.data());
  EXPECT_EQ(sacn_send_unicast_fake.call_count, (2u * kTestRemoteAddrs.size()) - num_terminating);
}

TEST_F(TestSourceState, SendUniverseMulticastWorks)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t universe_id, sacn_ip_support_t ip_supported,
                                            const uint8_t* send_buf, const EtcPalMcastNetintId* netint) {
    EXPECT_EQ(universe_id, kTestUniverseConfig.universe);
    EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);
    EXPECT_EQ(memcmp(send_buf, kTestBuffer.data(), kTestBuffer.size()), 0);
    EXPECT_EQ(netint->index, kTestNetints[current_netint_index].iface.index);
    EXPECT_EQ(netint->ip_type, kTestNetints[current_netint_index].iface.ip_type);
    ++current_netint_index;
  };

  sacn_source_t source = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  uint16_t multicast_universe = AddUniverse(source, universe_config);
  universe_config.send_unicast_only = true;
  ++universe_config.universe;
  uint16_t unicast_only_universe = AddUniverse(source, universe_config);

  current_remote_addr_index = 0;
  send_universe_multicast(GetSource(source), GetUniverse(source, unicast_only_universe), kTestBuffer.data());
  EXPECT_EQ(sacn_send_multicast_fake.call_count, 0u);
  send_universe_multicast(GetSource(source), GetUniverse(source, multicast_universe), kTestBuffer.data());
  EXPECT_EQ(sacn_send_multicast_fake.call_count, kTestNetints.size());
}

TEST_F(TestSourceState, SetPreviewFlagWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, universe, kTestBuffer, kTestBuffer2);

  etcpal_getms_fake.return_val = kTestGetMsValue;

  set_preview_flag(GetSource(source), GetUniverse(source, universe), true);

  EXPECT_EQ(GetUniverse(source, universe)->send_preview, true);
  EXPECT_NE(GetUniverse(source, universe)->level_send_buf[SACN_OPTS_OFFSET] & SACN_OPTVAL_PREVIEW, 0x00u);
  EXPECT_NE(GetUniverse(source, universe)->pap_send_buf[SACN_OPTS_OFFSET] & SACN_OPTVAL_PREVIEW, 0x00u);
  EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.reset_time, kTestGetMsValue);
  EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.reset_time, kTestGetMsValue);

  etcpal_getms_fake.return_val = kTestGetMsValue2;

  set_preview_flag(GetSource(source), GetUniverse(source, universe), false);

  EXPECT_EQ(GetUniverse(source, universe)->send_preview, false);
  EXPECT_EQ(GetUniverse(source, universe)->level_send_buf[SACN_OPTS_OFFSET] & SACN_OPTVAL_PREVIEW, 0x00u);
  EXPECT_EQ(GetUniverse(source, universe)->pap_send_buf[SACN_OPTS_OFFSET] & SACN_OPTVAL_PREVIEW, 0x00u);
  EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.reset_time, kTestGetMsValue2);
  EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.reset_time, kTestGetMsValue2);
}

TEST_F(TestSourceState, SetUniversePriorityWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, universe, kTestBuffer, kTestBuffer2);

  etcpal_getms_fake.return_val = kTestGetMsValue;
  for (uint8_t priority = 1u; priority < 10u; ++priority)
  {
    set_universe_priority(GetSource(source), GetUniverse(source, universe), priority);

    EXPECT_EQ(GetUniverse(source, universe)->priority, priority);
    EXPECT_EQ(GetUniverse(source, universe)->level_send_buf[SACN_PRI_OFFSET], priority);
    EXPECT_EQ(GetUniverse(source, universe)->pap_send_buf[SACN_PRI_OFFSET], priority);
    EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.reset_time, etcpal_getms_fake.return_val);
    EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.reset_time, etcpal_getms_fake.return_val);

    ++etcpal_getms_fake.return_val;
  }
}

TEST_F(TestSourceState, SetUnicastDestTerminatingWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  AddTestUnicastDests(source, universe);

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
  {
    set_unicast_dest_terminating(&GetUniverse(source, universe)->unicast_dests[i], kTerminateAndRemove);
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].termination_state, kTerminatingAndRemoving);
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].num_terminations_sent, 0);

    GetUniverse(source, universe)->unicast_dests[i].num_terminations_sent = 2;

    set_unicast_dest_terminating(&GetUniverse(source, universe)->unicast_dests[i], kTerminateAndRemove);
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].termination_state, kTerminatingAndRemoving);
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].num_terminations_sent, 2);

    GetUniverse(source, universe)->unicast_dests[i].termination_state = kNotTerminating;

    set_unicast_dest_terminating(&GetUniverse(source, universe)->unicast_dests[i], kTerminateAndRemove);
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].termination_state, kTerminatingAndRemoving);
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].num_terminations_sent, 0);
  }
}

TEST_F(TestSourceState, ResetLevelAndPapTransmissionSuppressionWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, universe, kTestBuffer, kTestBuffer2);

  GetUniverse(source, universe)->level_packets_sent_before_suppression = 4;
  GetUniverse(source, universe)->pap_packets_sent_before_suppression = 4;
  GetUniverse(source, universe)->level_keep_alive_timer.reset_time = 0u;
  GetUniverse(source, universe)->level_keep_alive_timer.interval = 0u;
  GetUniverse(source, universe)->pap_keep_alive_timer.reset_time = 0u;
  GetUniverse(source, universe)->pap_keep_alive_timer.interval = 0u;

  etcpal_getms_fake.return_val = kTestGetMsValue;
  reset_transmission_suppression(GetSource(source), GetUniverse(source, universe), kResetLevelAndPap);

  EXPECT_EQ(GetUniverse(source, universe)->level_packets_sent_before_suppression, 0);
  EXPECT_EQ(GetUniverse(source, universe)->pap_packets_sent_before_suppression, 0);
  EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.reset_time, etcpal_getms_fake.return_val);
  EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.reset_time, etcpal_getms_fake.return_val);
  EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.interval,
            (uint32_t)kTestSourceConfig.keep_alive_interval);
  EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.interval,
            (uint32_t)kTestSourceConfig.keep_alive_interval);
}

TEST_F(TestSourceState, ResetLevelTransmissionSuppressionWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, universe, kTestBuffer, kTestBuffer2);

  GetUniverse(source, universe)->level_packets_sent_before_suppression = 4;
  GetUniverse(source, universe)->pap_packets_sent_before_suppression = 4;
  GetUniverse(source, universe)->level_keep_alive_timer.reset_time = 0u;
  GetUniverse(source, universe)->level_keep_alive_timer.interval = 0u;
  GetUniverse(source, universe)->pap_keep_alive_timer.reset_time = 0u;
  GetUniverse(source, universe)->pap_keep_alive_timer.interval = 0u;

  etcpal_getms_fake.return_val = kTestGetMsValue;
  reset_transmission_suppression(GetSource(source), GetUniverse(source, universe), kResetLevel);

  EXPECT_EQ(GetUniverse(source, universe)->level_packets_sent_before_suppression, 0);
  EXPECT_EQ(GetUniverse(source, universe)->pap_packets_sent_before_suppression, 4);
  EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.reset_time, etcpal_getms_fake.return_val);
  EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.reset_time, 0u);
  EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.interval,
            (uint32_t)kTestSourceConfig.keep_alive_interval);
  EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.interval, 0u);
}

TEST_F(TestSourceState, ResetPapTransmissionSuppressionWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, universe, kTestBuffer, kTestBuffer2);

  GetUniverse(source, universe)->level_packets_sent_before_suppression = 4;
  GetUniverse(source, universe)->pap_packets_sent_before_suppression = 4;
  GetUniverse(source, universe)->level_keep_alive_timer.reset_time = 0u;
  GetUniverse(source, universe)->level_keep_alive_timer.interval = 0u;
  GetUniverse(source, universe)->pap_keep_alive_timer.reset_time = 0u;
  GetUniverse(source, universe)->pap_keep_alive_timer.interval = 0u;

  etcpal_getms_fake.return_val = kTestGetMsValue;
  reset_transmission_suppression(GetSource(source), GetUniverse(source, universe), kResetPap);

  EXPECT_EQ(GetUniverse(source, universe)->level_packets_sent_before_suppression, 4);
  EXPECT_EQ(GetUniverse(source, universe)->pap_packets_sent_before_suppression, 0);
  EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.reset_time, 0u);
  EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.reset_time, etcpal_getms_fake.return_val);
  EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.interval, 0u);
  EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.interval,
            (uint32_t)kTestSourceConfig.keep_alive_interval);
}

TEST_F(TestSourceState, SetUniverseTerminatingWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  AddTestUnicastDests(source, universe);

  set_universe_terminating(GetUniverse(source, universe), kTerminateAndRemove);
  EXPECT_EQ(GetUniverse(source, universe)->termination_state, kTerminatingAndRemoving);
  EXPECT_EQ(GetUniverse(source, universe)->num_terminations_sent, 0);

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].termination_state, kTerminatingAndRemoving);

  GetUniverse(source, universe)->num_terminations_sent = 2;

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
    GetUniverse(source, universe)->unicast_dests[i].num_terminations_sent = 2;

  set_universe_terminating(GetUniverse(source, universe), kTerminateAndRemove);
  EXPECT_EQ(GetUniverse(source, universe)->termination_state, kTerminatingAndRemoving);
  EXPECT_EQ(GetUniverse(source, universe)->num_terminations_sent, 2);

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].num_terminations_sent, 2);

  GetUniverse(source, universe)->termination_state = kNotTerminating;

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
    GetUniverse(source, universe)->unicast_dests[i].termination_state = kNotTerminating;

  set_universe_terminating(GetUniverse(source, universe), kTerminateAndRemove);
  EXPECT_EQ(GetUniverse(source, universe)->termination_state, kTerminatingAndRemoving);
  EXPECT_EQ(GetUniverse(source, universe)->num_terminations_sent, 0);

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
  {
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].termination_state, kTerminatingAndRemoving);
    EXPECT_EQ(GetUniverse(source, universe)->unicast_dests[i].num_terminations_sent, 0);
  }
}

TEST_F(TestSourceState, SetSourceTerminatingWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int i = 0; i < 3; ++i)
  {
    AddUniverse(source, universe_config);
    ++universe_config.universe;
  }

  set_source_terminating(GetSource(source));
  EXPECT_EQ(GetSource(source)->terminating, true);
  for (uint16_t universe = kTestUniverseConfig.universe; universe < (kTestUniverseConfig.universe + 3u); ++universe)
  {
    EXPECT_EQ(GetUniverse(source, universe)->termination_state, kTerminatingAndRemoving);
    EXPECT_EQ(GetUniverse(source, universe)->num_terminations_sent, 0);

    GetUniverse(source, universe)->num_terminations_sent = 2;
  }

  set_source_terminating(GetSource(source));
  EXPECT_EQ(GetSource(source)->terminating, true);
  for (uint16_t universe = kTestUniverseConfig.universe; universe < (kTestUniverseConfig.universe + 3u); ++universe)
  {
    EXPECT_EQ(GetUniverse(source, universe)->termination_state, kTerminatingAndRemoving);
    EXPECT_EQ(GetUniverse(source, universe)->num_terminations_sent, 2);

    GetUniverse(source, universe)->termination_state = kNotTerminating;
  }

  GetSource(source)->terminating = false;

  set_source_terminating(GetSource(source));
  EXPECT_EQ(GetSource(source)->terminating, true);
  for (uint16_t universe = kTestUniverseConfig.universe; universe < (kTestUniverseConfig.universe + 3u); ++universe)
  {
    EXPECT_EQ(GetUniverse(source, universe)->termination_state, kTerminatingAndRemoving);
    EXPECT_EQ(GetUniverse(source, universe)->num_terminations_sent, 0);
  }
}

TEST_F(TestSourceState, SetSourceNameWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int i = 0; i < 3; ++i)
  {
    AddUniverse(source, universe_config);
    InitTestData(source, universe_config.universe, kTestBuffer, kTestBuffer2);
    ++universe_config.universe;
  }

  etcpal_getms_fake.return_val = kTestGetMsValue;

  set_source_name(GetSource(source), kTestName.c_str());
  EXPECT_EQ(strcmp(GetSource(source)->name, kTestName.c_str()), 0);

  char* name_in_discovery_buffer = (char*)(&GetSource(source)->universe_discovery_send_buf[SACN_SOURCE_NAME_OFFSET]);
  EXPECT_EQ(strncmp(name_in_discovery_buffer, kTestName.c_str(), kTestName.length()), 0);

  for (size_t i = kTestName.length(); i < (size_t)SACN_SOURCE_NAME_MAX_LEN; ++i)
  {
    EXPECT_EQ(GetSource(source)->name[i], '\0');
    EXPECT_EQ(name_in_discovery_buffer[i], '\0');
  }

  for (uint16_t universe = kTestUniverseConfig.universe; universe < (kTestUniverseConfig.universe + 3u); ++universe)
  {
    char* name_in_level_buffer = (char*)(&GetUniverse(source, universe)->level_send_buf[SACN_SOURCE_NAME_OFFSET]);
    char* name_in_pap_buffer = (char*)(&GetUniverse(source, universe)->pap_send_buf[SACN_SOURCE_NAME_OFFSET]);
    EXPECT_EQ(strncmp(name_in_level_buffer, kTestName.c_str(), kTestName.length()), 0);
    EXPECT_EQ(strncmp(name_in_pap_buffer, kTestName.c_str(), kTestName.length()), 0);

    for (size_t i = kTestName.length(); i < (size_t)SACN_SOURCE_NAME_MAX_LEN; ++i)
    {
      EXPECT_EQ(name_in_level_buffer[i], '\0');
      EXPECT_EQ(name_in_pap_buffer[i], '\0');
    }

    EXPECT_EQ(GetUniverse(source, universe)->level_keep_alive_timer.reset_time, kTestGetMsValue);
    EXPECT_EQ(GetUniverse(source, universe)->pap_keep_alive_timer.reset_time, kTestGetMsValue);
  }
}

TEST_F(TestSourceState, GetSourceUniversesWorks)
{
  const size_t kNumUniverses = 7u;
  const size_t kContainerSize = kNumUniverses * 2u;

  sacn_source_t source = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (size_t i = 0u; i < kNumUniverses; ++i)
  {
    AddUniverse(source, universe_config);
    ++universe_config.universe;
  }

  uint16_t universes[kContainerSize] = {0u};

  size_t num_universes = get_source_universes(GetSource(source), universes, 1u);
  EXPECT_EQ(num_universes, kNumUniverses);

  EXPECT_EQ(universes[0], kTestUniverseConfig.universe);
  for (uint16_t i = 1u; i < kContainerSize; ++i)
    EXPECT_EQ(universes[i], 0u);

  num_universes = get_source_universes(GetSource(source), universes, kContainerSize);
  EXPECT_EQ(num_universes, kNumUniverses);

  for (uint16_t i = 0u; i < kNumUniverses; ++i)
    EXPECT_EQ(universes[i], kTestUniverseConfig.universe + i);
  for (uint16_t i = kNumUniverses; i < kContainerSize; ++i)
    EXPECT_EQ(universes[i], 0u);

  size_t num_terminating = 0u;
  for (uint16_t universe = kTestUniverseConfig.universe; universe < (kTestUniverseConfig.universe + kNumUniverses);
       universe += 2u)
  {
    set_universe_terminating(GetUniverse(source, universe), kTerminateAndRemove);
    ++num_terminating;
  }

  num_universes = get_source_universes(GetSource(source), universes, kContainerSize);
  EXPECT_EQ(num_universes, kNumUniverses - num_terminating);

  for (uint16_t i = 0u; i < (kNumUniverses - num_terminating); ++i)
    EXPECT_EQ(universes[i], kTestUniverseConfig.universe + (i * 2u) + 1u);
}

TEST_F(TestSourceState, GetSourceUnicastDestsWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  AddTestUnicastDests(source, universe);

  EtcPalIpAddr invalid_addr = ETCPAL_IP_INVALID_INIT;
  std::vector<EtcPalIpAddr> destinations(kTestRemoteAddrs.size() * 2u);
  for (size_t i = 0u; i < (kTestRemoteAddrs.size() * 2u); ++i)
    destinations[i] = invalid_addr;

  size_t num_dests = get_source_unicast_dests(GetUniverse(source, universe), destinations.data(), 1u);
  EXPECT_EQ(num_dests, kTestRemoteAddrs.size());

  EXPECT_EQ(etcpal_ip_cmp(&destinations[0], &kTestRemoteAddrs[0]), 0);
  for (size_t i = 1u; i < destinations.size(); ++i)
    EXPECT_EQ(etcpal_ip_cmp(&destinations[i], &invalid_addr), 0);

  num_dests = get_source_unicast_dests(GetUniverse(source, universe), destinations.data(), destinations.size());
  EXPECT_EQ(num_dests, kTestRemoteAddrs.size());

  for (size_t i = 0u; i < kTestRemoteAddrs.size(); ++i)
    EXPECT_EQ(etcpal_ip_cmp(&destinations[i], &kTestRemoteAddrs[i]), 0);
  for (size_t i = kTestRemoteAddrs.size(); i < destinations.size(); ++i)
    EXPECT_EQ(etcpal_ip_cmp(&destinations[i], &invalid_addr), 0);

  size_t num_terminating = 0u;
  for (size_t i = 0u; i < kTestRemoteAddrs.size(); i += 2u)
  {
    set_unicast_dest_terminating(&GetUniverse(source, universe)->unicast_dests[i], kTerminateAndRemove);
    ++num_terminating;
  }

  num_dests = get_source_unicast_dests(GetUniverse(source, universe), destinations.data(), destinations.size());
  EXPECT_EQ(num_dests, kTestRemoteAddrs.size() - num_terminating);

  for (size_t i = 0u; i < (kTestRemoteAddrs.size() - num_terminating); ++i)
    EXPECT_EQ(etcpal_ip_cmp(&destinations[i], &kTestRemoteAddrs[(i * 2) + 1]), 0);
}

TEST_F(TestSourceState, GetSourceUniverseNetintsWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);

  std::vector<EtcPalMcastNetintId> netints(kTestNetints.size() * 2u);
  for (size_t i = 0u; i < (kTestNetints.size() * 2u); ++i)
  {
    netints[i].index = 0u;
    netints[i].ip_type = kEtcPalIpTypeInvalid;
  }

  size_t num_netints = get_source_universe_netints(GetUniverse(source, universe), netints.data(), 1u);
  EXPECT_EQ(num_netints, kTestNetints.size());

  EXPECT_EQ(netints[0].index, kTestNetints[0].iface.index);
  EXPECT_EQ(netints[0].ip_type, kTestNetints[0].iface.ip_type);
  for (size_t i = 1u; i < netints.size(); ++i)
  {
    EXPECT_EQ(netints[i].index, 0u);
    EXPECT_EQ(netints[i].ip_type, kEtcPalIpTypeInvalid);
  }

  num_netints = get_source_universe_netints(GetUniverse(source, universe), netints.data(), netints.size());
  EXPECT_EQ(num_netints, kTestNetints.size());

  for (size_t i = 0u; i < kTestNetints.size(); ++i)
  {
    EXPECT_EQ(netints[i].index, kTestNetints[i].iface.index);
    EXPECT_EQ(netints[i].ip_type, kTestNetints[i].iface.ip_type);
  }

  for (size_t i = kTestNetints.size(); i < netints.size(); ++i)
  {
    EXPECT_EQ(netints[i].index, 0u);
    EXPECT_EQ(netints[i].ip_type, kEtcPalIpTypeInvalid);
  }
}

TEST_F(TestSourceState, DisablePapDataWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  uint16_t universe = AddUniverse(source, kTestUniverseConfig);
  InitTestData(source, universe, kTestBuffer, kTestBuffer2);

  EXPECT_EQ(GetUniverse(source, universe)->has_pap_data, true);
  disable_pap_data(GetUniverse(source, universe));
  EXPECT_EQ(GetUniverse(source, universe)->has_pap_data, false);
}

TEST_F(TestSourceState, ClearSourceNetintsWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);
  AddUniverse(source, kTestUniverseConfig);

  EXPECT_EQ(GetSource(source)->num_netints, kTestNetints.size());
  clear_source_netints(GetSource(source));
  EXPECT_EQ(GetSource(source)->num_netints, 0u);
}

TEST_F(TestSourceState, ResetSourceUniverseNetworkingWorks)
{
  sacn_source_t source = AddSource(kTestSourceConfig);

  SacnSourceUniverse* universe_state = nullptr;
  EXPECT_EQ(add_sacn_source_universe(GetSource(source), &kTestUniverseConfig, kTestNetints.data(), kTestNetints.size(),
                                     &universe_state),
            kEtcPalErrOk);
  InitTestData(source, kTestUniverseConfig.universe, kTestBuffer, kTestBuffer2);

  CLEAR_BUF(&universe_state->netints, netints);

  EXPECT_EQ(GetSource(source)->num_netints, 0u);

  etcpal_getms_fake.return_val = kTestGetMsValue;

  EXPECT_EQ(
      reset_source_universe_networking(GetSource(source), universe_state, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrOk);
  EXPECT_EQ(universe_state->netints.num_netints, kTestNetints.size());
  EXPECT_EQ(GetSource(source)->num_netints, kTestNetints.size());

  for (size_t i = 0u; i < kTestNetints.size(); ++i)
  {
    EXPECT_EQ(universe_state->netints.netints[i].index, kTestNetints[i].iface.index);
    EXPECT_EQ(universe_state->netints.netints[i].ip_type, kTestNetints[i].iface.ip_type);
    EXPECT_EQ(GetSource(source)->netints[i].id.index, kTestNetints[i].iface.index);
    EXPECT_EQ(GetSource(source)->netints[i].id.ip_type, kTestNetints[i].iface.ip_type);
    EXPECT_EQ(GetSource(source)->netints[i].num_refs, 1u);
  }

  EXPECT_EQ(universe_state->level_keep_alive_timer.reset_time, kTestGetMsValue);
  EXPECT_EQ(universe_state->pap_keep_alive_timer.reset_time, kTestGetMsValue);
}
