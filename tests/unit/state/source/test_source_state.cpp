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

#include "sacn/private/source_state.h"

#include <limits>
#include <optional>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
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

#define NUM_TEST_NETINTS 3u
#define NUM_TEST_ADDRS 4u
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

static const etcpal::Uuid kTestLocalCid = etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22");
static const std::string kTestLocalName = std::string("Test Source");
static const SacnSourceConfig kTestSourceConfig = {kTestLocalCid.get(),
                                                   kTestLocalName.c_str(),
                                                   SACN_SOURCE_INFINITE_UNIVERSES,
                                                   false,
                                                   kSacnIpV4AndIpV6,
                                                   SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT};
static const SacnSourceUniverseConfig kTestUniverseConfig = {1, 100, false, false, NULL, 0, 0};
static SacnMcastInterface kTestNetints[NUM_TEST_NETINTS] = {{{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},
                                                            {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
                                                            {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}};
static const uint8_t* kTestBuffer = (uint8_t*)"ABCDEFGHIJKL";
static const size_t kTestBufferLength = strlen((char*)kTestBuffer);
static const EtcPalIpAddr kTestRemoteAddrs[NUM_TEST_ADDRS] = {
    etcpal::IpAddr::FromString("10.101.1.1").get(), etcpal::IpAddr::FromString("10.101.1.2").get(),
    etcpal::IpAddr::FromString("10.101.1.3").get(), etcpal::IpAddr::FromString("10.101.1.4").get()};

// Some of the tests use these variables to communicate with their custom_fake lambdas.
static int current_test_iteration = 0;
static int current_remote_addr_index = 0;

class TestSourceState : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_sockets_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_source_state_init(), kEtcPalErrOk);
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
                       SacnMcastInterface* netints = kTestNetints, size_t num_netints = NUM_TEST_NETINTS)
  {
    SacnSourceUniverse* tmp = nullptr;
    EXPECT_EQ(add_sacn_source_universe(GetSource(source), &config, netints, num_netints, &tmp), kEtcPalErrOk);

    for (size_t i = 0; i < num_netints; ++i)
      EXPECT_EQ(add_sacn_source_netint(GetSource(source), &netints[i].iface), kEtcPalErrOk);

    return config.universe;
  }

  SacnSourceUniverse* GetUniverse(sacn_source_t source, uint16_t universe)
  {
    SacnSource* source_state = nullptr;
    SacnSourceUniverse* universe_state = nullptr;
    lookup_source_and_universe(source, universe, &source_state, &universe_state);
    return universe_state;
  }

  void InitTestLevels(sacn_source_t source, uint16_t universe, const uint8_t* levels, size_t levels_size)
  {
    update_levels_and_or_paps(GetSource(source), GetUniverse(source, universe), levels, levels_size, nullptr, 0u,
                              kDisableForceSync);
  }

  void AddUniverseForUniverseDiscovery(sacn_source_t source_handle, SacnSourceUniverseConfig& universe_config,
                                       SacnMcastInterface* netints = kTestNetints,
                                       size_t num_netints = NUM_TEST_NETINTS)
  {
    AddUniverse(source_handle, universe_config, netints, num_netints);
    InitTestLevels(source_handle, universe_config.universe, kTestBuffer, kTestBufferLength);
    ++universe_config.universe;
  }

  void AddTestUnicastDests(sacn_source_t source, uint16_t universe)
  {
    SacnUnicastDestination* tmp = nullptr;
    for (int i = 0; i < NUM_TEST_ADDRS; ++i)
      EXPECT_EQ(add_sacn_unicast_dest(GetUniverse(source, universe), &kTestRemoteAddrs[i], &tmp), kEtcPalErrOk);
  }

  sacn_source_t next_source_handle_ = 0;
};

TEST_F(TestSourceState, ProcessSourcesCountsSources)
{
  SacnSourceConfig config = kTestSourceConfig;

  config.manually_process_source = true;
  AddSource(config);
  AddSource(config);
  AddSource(config);
  int num_manual_sources = get_num_sources();

  config.manually_process_source = false;
  AddSource(config);
  AddSource(config);
  int num_threaded_sources = (get_num_sources() - num_manual_sources);

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

  InitTestLevels(threaded_source_1, kTestUniverseConfig.universe, kTestBuffer, kTestBufferLength);
  InitTestLevels(threaded_source_2, kTestUniverseConfig.universe, kTestBuffer, kTestBufferLength);

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

  sacn_source_t source_handle = AddSource(kTestSourceConfig);
  AddUniverse(source_handle, kTestUniverseConfig);
  InitTestLevels(source_handle, kTestUniverseConfig.universe, kTestBuffer, kTestBufferLength);

  for (int i = 0; i < 10; ++i)
  {
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
    EXPECT_EQ(sacn_send_multicast_fake.call_count, NUM_TEST_NETINTS * i);

    etcpal_getms_fake.return_val += SACN_UNIVERSE_DISCOVERY_INTERVAL;

    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
    EXPECT_EQ(sacn_send_multicast_fake.call_count, NUM_TEST_NETINTS * i);

    ++etcpal_getms_fake.return_val;
  }
}

TEST_F(TestSourceState, SourceTerminatingStopsUniverseDiscovery)
{
  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);
  AddUniverse(source_handle, kTestUniverseConfig);
  InitTestLevels(source_handle, kTestUniverseConfig.universe, kTestBuffer, kTestBufferLength);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  EXPECT_EQ(sacn_send_multicast_fake.call_count, 0u);

  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  EXPECT_EQ(sacn_send_multicast_fake.call_count, NUM_TEST_NETINTS);

  set_source_terminating(GetSource(source_handle));
  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);

  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
  EXPECT_EQ(sacn_send_multicast_fake.call_count, NUM_TEST_NETINTS);
  EXPECT_EQ(get_num_sources(), 1u);
}

TEST_F(TestSourceState, UniverseDiscoverySendsForEachPage)
{
  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  int last_send_count = sacn_send_multicast_fake.call_count;
  for (int num_pages = 1; num_pages <= 4; ++num_pages)
  {
    for (int i = 0; i < SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE; ++i)
      AddUniverseForUniverseDiscovery(source_handle, universe_config);

    etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);

    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
    EXPECT_EQ(sacn_send_multicast_fake.call_count - last_send_count, num_pages * NUM_TEST_NETINTS);

    last_send_count = sacn_send_multicast_fake.call_count;
  }
}

TEST_F(TestSourceState, UniverseDiscoverySendsCorrectUniverseLists)
{
  ASSERT_EQ(SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE % 4, 0);

  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
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
    EXPECT_EQ(send_buf[SACN_UNIVERSE_DISCOVERY_PAGE_OFFSET],
              (sacn_send_multicast_fake.call_count - 1) / NUM_TEST_NETINTS);
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
      EXPECT_EQ(send_buf[SACN_UNIVERSE_DISCOVERY_LAST_PAGE_OFFSET], current_test_iteration);
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
    EXPECT_EQ(send_buf[SACN_SEQ_OFFSET], (sacn_send_multicast_fake.call_count - 1) / NUM_TEST_NETINTS);
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
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t*,
                                            const EtcPalMcastNetintId* netint) {
    EXPECT_EQ(netint->ip_type, kTestNetints[sacn_send_multicast_fake.call_count - 1].iface.ip_type);
    EXPECT_EQ(netint->index, kTestNetints[sacn_send_multicast_fake.call_count - 1].iface.index);
  };

  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int i = 0; i < NUM_TEST_NETINTS; ++i)
    AddUniverseForUniverseDiscovery(source_handle, universe_config, &kTestNetints[i], 1u);

  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

  EXPECT_EQ(sacn_send_multicast_fake.call_count, NUM_TEST_NETINTS);
}

TEST_F(TestSourceState, UniverseDiscoveryExcludesUniversesWithoutData)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    int num_universes = (ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE])) + ACN_UDP_PREAMBLE_SIZE -
                         SACN_UNIVERSE_DISCOVERY_HEADER_SIZE) /
                        2;

    for (int i = 0; i < num_universes; ++i)
    {
      int universe = etcpal_unpack_u16b(&send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (i * 2)]);
      EXPECT_EQ(universe % 2, 0);
    }
  };

  etcpal_getms_fake.return_val = 0u;

  sacn_source_t source_handle = AddSource(kTestSourceConfig);

  SacnSourceUniverseConfig universe_config = kTestUniverseConfig;
  for (int i = 0; i < 100; ++i)
  {
    AddUniverse(source_handle, universe_config);

    if (i % 2)
      InitTestLevels(source_handle, universe_config.universe, kTestBuffer, kTestBufferLength);

    ++universe_config.universe;
  }

  etcpal_getms_fake.return_val += (SACN_UNIVERSE_DISCOVERY_INTERVAL + 1u);
  VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));
}

TEST_F(TestSourceState, UniverseDiscoveryExcludesUnicastOnlyUniverses)
{
  sacn_send_multicast_fake.custom_fake = [](uint16_t, sacn_ip_support_t, const uint8_t* send_buf,
                                            const EtcPalMcastNetintId*) {
    int num_universes = (ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE])) + ACN_UDP_PREAMBLE_SIZE -
                         SACN_UNIVERSE_DISCOVERY_HEADER_SIZE) /
                        2;

    for (int i = 0; i < num_universes; ++i)
    {
      int universe = etcpal_unpack_u16b(&send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE + (i * 2)]);
      EXPECT_EQ(universe % 2, 1);
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

TEST_F(TestSourceState, UnicastDestsWithDataTerminateCorrectly)
{
  sacn_send_unicast_fake.custom_fake = [](sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                                          const EtcPalIpAddr* dest_addr) {
    EXPECT_EQ(ip_supported, kTestSourceConfig.ip_supported);
    EXPECT_NE(TERMINATED_OPT_SET(send_buf), 0x00u);
    EXPECT_EQ(etcpal_ip_cmp(dest_addr, &kTestRemoteAddrs[current_remote_addr_index]), 0);

    --current_remote_addr_index;
  };

  sacn_source_t source = AddSource(kTestSourceConfig);
  AddUniverse(source, kTestUniverseConfig, kTestNetints, NUM_TEST_NETINTS);
  InitTestLevels(source, kTestUniverseConfig.universe, kTestBuffer, kTestBufferLength);
  AddTestUnicastDests(source, kTestUniverseConfig.universe);

  for (int i = 0; i < NUM_TEST_ADDRS; ++i)
    set_unicast_dest_terminating(&GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[i]);

  for (int i = 0; i < 3; ++i)
  {
    uint8_t old_seq_num = GetUniverse(source, kTestUniverseConfig.universe)->seq_num;

    current_remote_addr_index = (NUM_TEST_ADDRS - 1);
    VERIFY_LOCKING(take_lock_and_process_sources(kProcessThreadedSources));

    for (int j = 0; j < NUM_TEST_ADDRS; ++j)
      EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->unicast_dests[j].num_terminations_sent, i + 1);

    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->num_unicast_dests, (i < 2) ? NUM_TEST_ADDRS : 0u);
    EXPECT_EQ(GetUniverse(source, kTestUniverseConfig.universe)->seq_num - old_seq_num,
              (uint8_t)(NUM_TEST_ADDRS + 1u));  // One sequence number for each unicast termination packet + one more
                                                // for non-unicast, non-termination data.
    EXPECT_EQ(TERMINATED_OPT_SET(GetUniverse(source, kTestUniverseConfig.universe)->null_send_buf), 0x00u);
  }

  EXPECT_EQ(sacn_send_unicast_fake.call_count, NUM_TEST_ADDRS * 3u);
}
