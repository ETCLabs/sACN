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

#include "sacn/source.h"

#include <limits>
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/sockets.h"
#include "sacn_mock/private/source_state.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/pdu.h"
#include "sacn/private/source.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestSource TestSourceDynamic
#else
#define TestSource TestSourceStatic
#endif

#define NUM_TEST_NETINTS 3
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
static const std::string kTestLocalName2 = std::string("Test Source 2");
static const EtcPalIpAddr kTestRemoteAddrV4 = etcpal::IpAddr::FromString("10.101.1.1").get();
static const EtcPalIpAddr kTestRemoteAddrV6 = etcpal::IpAddr::FromString("2001:db8::1234:5678").get();
static const sacn_source_t kTestHandle = 123u;
static const uint16_t kTestUniverse = 456u;
static const uint8_t kTestPriority = 77u;
static const bool kTestPreviewFlag = true;
static const uint8_t kTestStartCode = 0x12u;
static const uint8_t* kTestBuffer = (uint8_t*)"ABCDEFGHIJKL";
static const size_t kTestBufferLength = strlen((char*)kTestBuffer);
static const uint8_t* kTestBuffer2 = (uint8_t*)"MNOPQRSTUVWXYZ";
static const size_t kTestBuffer2Length = strlen((char*)kTestBuffer2);
static SacnMcastInterface kTestNetints[NUM_TEST_NETINTS] = {{{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},
                                                            {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
                                                            {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}};
static const size_t kTestReturnSize = 1234u;
static const int kTestReturnInt = 5678;
static const SacnSourceUniverseNetintList kTestNetintList = {kTestHandle, kTestUniverse, kTestNetints,
                                                             NUM_TEST_NETINTS};

class TestSource : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_sockets_reset_all_fakes();
    sacn_source_state_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_source_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_source_deinit();
    sacn_mem_deinit();
  }

  void SetUpSource(sacn_source_t source_handle)
  {
    SacnSourceConfig source_config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
    source_config.cid = kTestLocalCid.get();
    source_config.name = kTestLocalName.c_str();

    get_next_source_handle_fake.return_val = source_handle;

    sacn_source_t handle = SACN_SOURCE_INVALID;
    EXPECT_EQ(sacn_source_create(&source_config, &handle), kEtcPalErrOk);
  }

  void SetUpSourceAndUniverse(sacn_source_t source_handle, uint16_t universe_id)
  {
    SetUpSource(source_handle);

    SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
    universe_config.universe = universe_id;

    EXPECT_EQ(sacn_source_add_universe(source_handle, &universe_config, kTestNetints, NUM_TEST_NETINTS), kEtcPalErrOk);
  }
};

TEST_F(TestSource, SourceConfigInitWorks)
{
  SacnSourceConfig config;
  sacn_source_config_init(&config);
  EXPECT_EQ(ETCPAL_UUID_CMP(&config.cid, &kEtcPalNullUuid), 0);
  EXPECT_EQ(config.name, (char*)NULL);
  EXPECT_EQ(config.universe_count_max, (size_t)SACN_SOURCE_INFINITE_UNIVERSES);
  EXPECT_EQ(config.manually_process_source, false);
  EXPECT_EQ(config.ip_supported, kSacnIpV4AndIpV6);
  EXPECT_EQ(config.keep_alive_interval, SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT);
}

TEST_F(TestSource, SourceUniverseConfigInitWorks)
{
  SacnSourceUniverseConfig config;
  sacn_source_universe_config_init(&config);
  EXPECT_EQ(config.universe, 0u);
  EXPECT_EQ(config.priority, 100u);
  EXPECT_EQ(config.send_preview, false);
  EXPECT_EQ(config.send_unicast_only, false);
  EXPECT_EQ(config.unicast_destinations, (EtcPalIpAddr*)NULL);
  EXPECT_EQ(config.num_unicast_destinations, 0u);
  EXPECT_EQ(config.sync_universe, 0u);
}

TEST_F(TestSource, ThreadedSourceCreateWorks)
{
  SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  config.cid = kTestLocalCid.get();
  config.name = kTestLocalName.c_str();
  config.manually_process_source = false;

  get_next_source_handle_fake.return_val = kTestHandle;

  sacn_source_t handle = SACN_SOURCE_INVALID;
  SacnSource* source_state = nullptr;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&config, &handle), kEtcPalErrOk);
  EXPECT_EQ(initialize_source_thread_fake.call_count, 1u);
  EXPECT_EQ(get_next_source_handle_fake.call_count, 1u);
  EXPECT_EQ(lookup_source(kTestHandle, &source_state), kEtcPalErrOk);
  EXPECT_EQ(handle, kTestHandle);
}

TEST_F(TestSource, ManualSourceCreateWorks)
{
  SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  config.cid = kTestLocalCid.get();
  config.name = kTestLocalName.c_str();
  config.manually_process_source = true;

  get_next_source_handle_fake.return_val = kTestHandle;

  sacn_source_t handle = SACN_SOURCE_INVALID;
  SacnSource* source_state = nullptr;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&config, &handle), kEtcPalErrOk);
  EXPECT_EQ(initialize_source_thread_fake.call_count, 0u);  // This should not be called for manual sources.
  EXPECT_EQ(get_next_source_handle_fake.call_count, 1u);
  EXPECT_EQ(lookup_source(kTestHandle, &source_state), kEtcPalErrOk);
  EXPECT_EQ(handle, kTestHandle);
}

TEST_F(TestSource, SourceDestroyWorks)
{
  SetUpSource(kTestHandle);

  set_source_terminating_fake.custom_fake = [](SacnSource* source) {
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->handle, kTestHandle);
  };

  VERIFY_LOCKING(sacn_source_destroy(kTestHandle));
  EXPECT_EQ(set_source_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceChangeNameWorks)
{
  SetUpSource(kTestHandle);

  set_source_name_fake.custom_fake = [](SacnSource* source, const char* new_name) {
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(strcmp(new_name, kTestLocalName2.c_str()), 0);
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, kTestLocalName2.c_str()), kEtcPalErrOk);
  EXPECT_EQ(set_source_name_fake.call_count, 1u);
}

TEST_F(TestSource, SourceAddUniverseWorks)
{
  SetUpSource(kTestHandle);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_initialize_source_netints_fake.custom_fake = [](SacnInternalNetintArray* source_netints,
                                                       SacnMcastInterface* app_netints, size_t num_app_netints) {
#if SACN_DYNAMIC_MEM
    source_netints->netints = (EtcPalMcastNetintId*)calloc(NUM_TEST_NETINTS, sizeof(EtcPalMcastNetintId));
    source_netints->netints_capacity = NUM_TEST_NETINTS;
#endif
    source_netints->num_netints = NUM_TEST_NETINTS;

    for (size_t i = 0u; i < num_app_netints; ++i)
    {
      EXPECT_EQ(app_netints[i].iface.index, kTestNetints[i].iface.index);
      EXPECT_EQ(app_netints[i].iface.ip_type, kTestNetints[i].iface.ip_type);
      EXPECT_EQ(app_netints[i].status, kTestNetints[i].status);
      source_netints->netints[i] = app_netints[i].iface;
    }

    return kEtcPalErrOk;
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints, NUM_TEST_NETINTS), kEtcPalErrOk);

  SacnSource* source = nullptr;
  SacnSourceUniverse* universe = nullptr;
  EXPECT_EQ(lookup_source_and_universe(kTestHandle, kTestUniverse, &source, &universe), kEtcPalErrOk);
  if (source)
  {
    EXPECT_EQ(source->num_netints, (size_t)NUM_TEST_NETINTS);
    for (size_t i = 0; (i < NUM_TEST_NETINTS) && (i < source->num_netints); ++i)
    {
      EXPECT_EQ(source->netints[i].id.index, kTestNetints[i].iface.index);
      EXPECT_EQ(source->netints[i].id.ip_type, kTestNetints[i].iface.ip_type);
    }
  }
}

TEST_F(TestSource, SourceRemoveUniverseWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  set_universe_terminating_fake.custom_fake = [](SacnSourceUniverse* universe) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
  };

  VERIFY_LOCKING(sacn_source_remove_universe(kTestHandle, kTestUniverse));
  EXPECT_EQ(set_universe_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceGetUniversesWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  get_source_universes_fake.custom_fake = [](const SacnSource* source, uint16_t* universes, size_t universes_size) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universes, nullptr);
    EXPECT_EQ(universes_size, 0u);
    return kTestReturnSize;
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_get_universes(kTestHandle, nullptr, 0u), kTestReturnSize);
  EXPECT_EQ(get_source_universes_fake.call_count, 1u);
}

TEST_F(TestSource, SourceAddUnicastDestinationWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  reset_transmission_suppression_fake.custom_fake = [](const SacnSource* source, SacnSourceUniverse* universe,
                                                       reset_transmission_suppression_behavior_t behavior) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universe->universe_id, kTestUniverse);
    EXPECT_EQ(behavior, kResetLevelAndPap);
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrV4),
                                  kEtcPalErrOk);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  SacnUnicastDestination* unicast_dest = nullptr;
  EXPECT_EQ(lookup_source_and_universe(kTestHandle, kTestUniverse, &source_state, &universe_state), kEtcPalErrOk);
  EXPECT_EQ(lookup_unicast_dest(universe_state, &kTestRemoteAddrV4, &unicast_dest), kEtcPalErrOk);

  EXPECT_EQ(reset_transmission_suppression_fake.call_count, 1u);
}

TEST_F(TestSource, SourceRemoveUnicastDestinationWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  set_unicast_dest_terminating_fake.custom_fake = [](SacnUnicastDestination* dest) {
    EXPECT_EQ(etcpal_ip_cmp(&dest->dest_addr, &kTestRemoteAddrV4), 0);
  };

  EXPECT_EQ(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrV4), kEtcPalErrOk);
  VERIFY_LOCKING(sacn_source_remove_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrV4));
  EXPECT_EQ(set_unicast_dest_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceGetUnicastDestinationsWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  get_source_unicast_dests_fake.custom_fake = [](const SacnSourceUniverse* universe, EtcPalIpAddr* destinations,
                                                 size_t destinations_size) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
    EXPECT_EQ(destinations, nullptr);
    EXPECT_EQ(destinations_size, 0u);
    return kTestReturnSize;
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_get_unicast_destinations(kTestHandle, kTestUniverse, nullptr, 0u),
                                  kTestReturnSize);
  EXPECT_EQ(get_source_unicast_dests_fake.call_count, 1u);
}

TEST_F(TestSource, SourceChangePriorityWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  set_universe_priority_fake.custom_fake = [](const SacnSource* source, SacnSourceUniverse* universe,
                                              uint8_t priority) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universe->universe_id, kTestUniverse);
    EXPECT_EQ(priority, kTestPriority);
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, kTestUniverse, kTestPriority), kEtcPalErrOk);
  EXPECT_EQ(set_universe_priority_fake.call_count, 1u);
}

TEST_F(TestSource, SourceChangePreviewFlagWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  set_preview_flag_fake.custom_fake = [](const SacnSource* source, SacnSourceUniverse* universe, bool preview) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universe->universe_id, kTestUniverse);
    EXPECT_EQ(preview, kTestPreviewFlag);
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, kTestUniverse, kTestPreviewFlag),
                                  kEtcPalErrOk);
  EXPECT_EQ(set_preview_flag_fake.call_count, 1u);
}

TEST_F(TestSource, SourceSendNowWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  send_universe_multicast_fake.custom_fake = [](const SacnSource* source, SacnSourceUniverse* universe,
                                                const uint8_t* send_buf) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universe->universe_id, kTestUniverse);
    EXPECT_EQ(send_buf[SACN_DATA_HEADER_SIZE - 1], kTestStartCode);
    EXPECT_EQ(memcmp(&send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer, kTestBufferLength), 0);
  };
  send_universe_unicast_fake.custom_fake = [](const SacnSource* source, SacnSourceUniverse* universe,
                                              const uint8_t* send_buf, send_universe_unicast_behavior_t behavior) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universe->universe_id, kTestUniverse);
    EXPECT_EQ(send_buf[SACN_DATA_HEADER_SIZE - 1], kTestStartCode);
    EXPECT_EQ(memcmp(&send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer, kTestBufferLength), 0);
    EXPECT_EQ(behavior, kSkipTerminatingUnicastDests);
  };
  increment_sequence_number_fake.custom_fake = [](SacnSourceUniverse* universe) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer, kTestBufferLength), kEtcPalErrOk);

  EXPECT_EQ(send_universe_multicast_fake.call_count, 1u);
  EXPECT_EQ(send_universe_unicast_fake.call_count, 1u);
  EXPECT_EQ(increment_sequence_number_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  update_levels_and_or_paps_fake.custom_fake =
      [](SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_levels, size_t new_levels_size,
         const uint8_t* new_priorities, size_t new_priorities_size, force_sync_behavior_t force_sync) {
        EXPECT_EQ(source->handle, kTestHandle);
        EXPECT_EQ(universe->universe_id, kTestUniverse);
        EXPECT_EQ(memcmp(new_levels, kTestBuffer, kTestBufferLength), 0);
        EXPECT_EQ(new_levels_size, kTestBufferLength);
        EXPECT_EQ(new_priorities, nullptr);
        EXPECT_EQ(new_priorities_size, 0u);
        EXPECT_EQ(force_sync, kDisableForceSync);
      };

  VERIFY_LOCKING(sacn_source_update_values(kTestHandle, kTestUniverse, kTestBuffer, kTestBufferLength));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndPapWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  update_levels_and_or_paps_fake.custom_fake =
      [](SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_levels, size_t new_levels_size,
         const uint8_t* new_priorities, size_t new_priorities_size, force_sync_behavior_t force_sync) {
        EXPECT_EQ(source->handle, kTestHandle);
        EXPECT_EQ(universe->universe_id, kTestUniverse);
        EXPECT_EQ(memcmp(new_levels, kTestBuffer, kTestBufferLength), 0);
        EXPECT_EQ(new_levels_size, kTestBufferLength);
        if (new_priorities)
        {
          EXPECT_EQ(memcmp(new_priorities, kTestBuffer2, kTestBuffer2Length), 0);
          EXPECT_EQ(new_priorities_size, kTestBuffer2Length);
        }
        EXPECT_EQ(force_sync, kDisableForceSync);
      };

  VERIFY_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer, kTestBufferLength,
                                                   kTestBuffer2, kTestBuffer2Length));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
  EXPECT_EQ(disable_pap_data_fake.call_count, 0u);

  // Check disabling PAP as well
  disable_pap_data_fake.custom_fake = [](SacnSourceUniverse* universe) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
  };
  VERIFY_LOCKING(
      sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer, kTestBufferLength, nullptr, 0u));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 2u);
  EXPECT_EQ(disable_pap_data_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndForceSyncWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  update_levels_and_or_paps_fake.custom_fake =
      [](SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_levels, size_t new_levels_size,
         const uint8_t* new_priorities, size_t new_priorities_size, force_sync_behavior_t force_sync) {
        EXPECT_EQ(source->handle, kTestHandle);
        EXPECT_EQ(universe->universe_id, kTestUniverse);
        EXPECT_EQ(memcmp(new_levels, kTestBuffer, kTestBufferLength), 0);
        EXPECT_EQ(new_levels_size, kTestBufferLength);
        EXPECT_EQ(new_priorities, nullptr);
        EXPECT_EQ(new_priorities_size, 0u);
        EXPECT_EQ(force_sync, kEnableForceSync);
      };

  VERIFY_LOCKING(sacn_source_update_values_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer, kTestBufferLength));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndPapAndForceSyncWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  update_levels_and_or_paps_fake.custom_fake =
      [](SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_levels, size_t new_levels_size,
         const uint8_t* new_priorities, size_t new_priorities_size, force_sync_behavior_t force_sync) {
        EXPECT_EQ(source->handle, kTestHandle);
        EXPECT_EQ(universe->universe_id, kTestUniverse);
        EXPECT_EQ(memcmp(new_levels, kTestBuffer, kTestBufferLength), 0);
        EXPECT_EQ(new_levels_size, kTestBufferLength);
        if (new_priorities)
        {
          EXPECT_EQ(memcmp(new_priorities, kTestBuffer2, kTestBuffer2Length), 0);
          EXPECT_EQ(new_priorities_size, kTestBuffer2Length);
        }
        EXPECT_EQ(force_sync, kEnableForceSync);
      };

  VERIFY_LOCKING(sacn_source_update_values_and_pap_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer,
                                                                  kTestBufferLength, kTestBuffer2, kTestBuffer2Length));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
  EXPECT_EQ(disable_pap_data_fake.call_count, 0u);

  // Check disabling PAP as well
  disable_pap_data_fake.custom_fake = [](SacnSourceUniverse* universe) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
  };
  VERIFY_LOCKING(sacn_source_update_values_and_pap_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer,
                                                                  kTestBufferLength, nullptr, 0u));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 2u);
  EXPECT_EQ(disable_pap_data_fake.call_count, 1u);
}

TEST_F(TestSource, SourceProcessManualWorks)
{
  take_lock_and_process_sources_fake.custom_fake = [](process_sources_behavior_t behavior) {
    EXPECT_EQ(behavior, kProcessManualSources);
    return kTestReturnInt;
  };

  EXPECT_EQ(sacn_source_process_manual(), kTestReturnInt);
  EXPECT_EQ(take_lock_and_process_sources_fake.call_count, 1u);
}

TEST_F(TestSource, SourceResetNetworkingWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  clear_source_netints_fake.custom_fake = [](SacnSource* source) { EXPECT_EQ(source->handle, kTestHandle); };
  reset_source_universe_networking_fake.custom_fake = [](SacnSource* source, SacnSourceUniverse* universe,
                                                         SacnMcastInterface* netints, size_t num_netints) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universe->universe_id, kTestUniverse);

    for (size_t i = 0u; i < num_netints; ++i)
    {
      EXPECT_EQ(netints[i].iface.index, kTestNetints[i].iface.index);
      EXPECT_EQ(netints[i].iface.ip_type, kTestNetints[i].iface.ip_type);
      EXPECT_EQ(netints[i].status, kTestNetints[i].status);
    }

    return kEtcPalErrOk;
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_reset_networking(kTestNetints, NUM_TEST_NETINTS), kEtcPalErrOk);

  EXPECT_EQ(sacn_sockets_reset_source_fake.call_count, 1u);
  EXPECT_EQ(clear_source_netints_fake.call_count, 1u);
  EXPECT_EQ(reset_source_universe_networking_fake.call_count, 1u);
}

TEST_F(TestSource, SourceResetNetworkingPerUniverseWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  clear_source_netints_fake.custom_fake = [](SacnSource* source) { EXPECT_EQ(source->handle, kTestHandle); };
  reset_source_universe_networking_fake.custom_fake = [](SacnSource* source, SacnSourceUniverse* universe,
                                                         SacnMcastInterface* netints, size_t num_netints) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universe->universe_id, kTestUniverse);

    for (size_t i = 0u; i < num_netints; ++i)
    {
      EXPECT_EQ(netints[i].iface.index, kTestNetints[i].iface.index);
      EXPECT_EQ(netints[i].iface.ip_type, kTestNetints[i].iface.ip_type);
      EXPECT_EQ(netints[i].status, kTestNetints[i].status);
    }

    return kEtcPalErrOk;
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_reset_networking_per_universe(&kTestNetintList, 1u), kEtcPalErrOk);

  EXPECT_EQ(sacn_sockets_reset_source_fake.call_count, 1u);
  EXPECT_EQ(clear_source_netints_fake.call_count, 1u);
  EXPECT_EQ(reset_source_universe_networking_fake.call_count, 1u);
}

TEST_F(TestSource, SourceGetNetintsWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  get_source_universe_netints_fake.custom_fake = [](const SacnSourceUniverse* universe, EtcPalMcastNetintId* netints,
                                                    size_t netints_size) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
    EXPECT_EQ(netints, nullptr);
    EXPECT_EQ(netints_size, 0u);
    return kTestReturnSize;
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_get_network_interfaces(kTestHandle, kTestUniverse, nullptr, 0u),
                                  kTestReturnSize);
  EXPECT_EQ(get_source_universe_netints_fake.call_count, 1u);
}
