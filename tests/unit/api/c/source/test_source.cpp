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
#include "sacn/private/source.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestSource TestSourceDynamic
#else
#define TestSource TestSourceStatic
#endif

#define NUM_TEST_NETINTS 3

static const etcpal::Uuid kTestLocalCid = etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22");
static const std::string kTestLocalName = std::string("Test Source");
static const std::string kTestLocalName2 = std::string("Test Source 2");
static const etcpal::SockAddr kTestRemoteAddrV4(etcpal::IpAddr::FromString("10.101.1.1"), 8888);
static const etcpal::SockAddr kTestRemoteAddrV6(etcpal::IpAddr::FromString("2001:db8::1234:5678"), 8888);
static const sacn_source_t kTestHandle = 123u;
static const uint16_t kTestUniverse = 456u;
static SacnMcastInterface kTestNetints[NUM_TEST_NETINTS] = {{{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},
                                                            {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
                                                            {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}};
static const size_t kTestReturnSize = 1234u;

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

  etcpal::Expected<sacn_source_t> AddSource()
  {
    SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
    config.cid = kTestLocalCid.get();
    config.name = kTestLocalName.c_str();

    sacn_source_t handle;
    etcpal_error_t err = sacn_source_create(&config, &handle);

    if (err == kEtcPalErrOk)
      return handle;
    else
      return err;
  }

  etcpal::Expected<uint16_t> AddUniverse(sacn_source_t source)
  {
    SacnSourceUniverseConfig config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;

    SacnSource* source_state;
    etcpal_error_t error = lookup_source(source, &source_state);

    if (error == kEtcPalErrOk)
    {
      config.universe = static_cast<uint16_t>((source_state->num_universes + 1));
      error = sacn_source_add_universe(source, &config, NULL, 0);
    }

    if (error == kEtcPalErrOk)
      return config.universe;
    else
      return error;
  }

  etcpal::Expected<etcpal::IpAddr> AddUnicastDestination(sacn_source_t source, uint16_t universe)
  {
    EtcPalIpAddr test_ip = kTestRemoteAddrV4.ip().get();

    SacnSource* source_state;
    SacnSourceUniverse* universe_state;
    etcpal_error_t error = lookup_source_and_universe(source, universe, &source_state, &universe_state);

    if (error == kEtcPalErrOk)
    {
      test_ip.addr.v4 += universe_state->num_unicast_dests;
      error = sacn_source_add_unicast_destination(source, universe, &test_ip);
    }

    if (error == kEtcPalErrOk)
      return etcpal::IpAddr(test_ip);
    else
      return error;
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
  unsigned int previous_lock_count = sacn_lock_fake.call_count;
  EXPECT_EQ(sacn_source_create(&config, &handle), kEtcPalErrOk);
  EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);
  EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);
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
  unsigned int previous_lock_count = sacn_lock_fake.call_count;
  EXPECT_EQ(sacn_source_create(&config, &handle), kEtcPalErrOk);
  EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);
  EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);
  EXPECT_EQ(initialize_source_thread_fake.call_count, 0u);  // This should not be called for manual sources.
  EXPECT_EQ(get_next_source_handle_fake.call_count, 1u);
  EXPECT_EQ(lookup_source(kTestHandle, &source_state), kEtcPalErrOk);
  EXPECT_EQ(handle, kTestHandle);
}

TEST_F(TestSource, SourceDestroyWorks)
{
  SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  config.cid = kTestLocalCid.get();
  config.name = kTestLocalName.c_str();

  get_next_source_handle_fake.return_val = kTestHandle;
  set_source_terminating_fake.custom_fake = [](SacnSource* source) {
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->handle, kTestHandle);
  };

  sacn_source_t handle = SACN_SOURCE_INVALID;
  EXPECT_EQ(sacn_source_create(&config, &handle), kEtcPalErrOk);
  unsigned int previous_lock_count = sacn_lock_fake.call_count;
  sacn_source_destroy(handle);
  EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);
  EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);
  EXPECT_EQ(set_source_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceChangeNameWorks)
{
  SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  config.cid = kTestLocalCid.get();
  config.name = kTestLocalName.c_str();

  get_next_source_handle_fake.return_val = kTestHandle;

  set_source_name_fake.custom_fake = [](SacnSource* source, const char* new_name) {
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(strcmp(new_name, kTestLocalName2.c_str()), 0);
  };

  sacn_source_t handle = SACN_SOURCE_INVALID;
  EXPECT_EQ(sacn_source_create(&config, &handle), kEtcPalErrOk);
  unsigned int previous_lock_count = sacn_lock_fake.call_count;
  EXPECT_EQ(sacn_source_change_name(handle, kTestLocalName2.c_str()), kEtcPalErrOk);
  EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);
  EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);
  EXPECT_EQ(set_source_name_fake.call_count, 1u);
}

TEST_F(TestSource, SourceAddUniverseWorks)
{
  SacnSourceConfig source_config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  source_config.cid = kTestLocalCid.get();
  source_config.name = kTestLocalName.c_str();

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  get_next_source_handle_fake.return_val = kTestHandle;

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

  sacn_source_t handle = SACN_SOURCE_INVALID;
  EXPECT_EQ(sacn_source_create(&source_config, &handle), kEtcPalErrOk);
  unsigned int previous_lock_count = sacn_lock_fake.call_count;
  EXPECT_EQ(sacn_source_add_universe(handle, &universe_config, kTestNetints, NUM_TEST_NETINTS), kEtcPalErrOk);
  EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);
  EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);

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
  SacnSourceConfig source_config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  source_config.cid = kTestLocalCid.get();
  source_config.name = kTestLocalName.c_str();

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_t handle = SACN_SOURCE_INVALID;
  EXPECT_EQ(sacn_source_create(&source_config, &handle), kEtcPalErrOk);
  EXPECT_EQ(sacn_source_add_universe(handle, &universe_config, kTestNetints, NUM_TEST_NETINTS), kEtcPalErrOk);
  unsigned int previous_lock_count = sacn_lock_fake.call_count;
  sacn_source_remove_universe(handle, kTestUniverse);
  EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);
  EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);
  EXPECT_EQ(set_universe_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceGetUniversesWorks)
{
  SacnSourceConfig source_config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  source_config.cid = kTestLocalCid.get();
  source_config.name = kTestLocalName.c_str();

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  get_next_source_handle_fake.return_val = kTestHandle;

  get_source_universes_fake.custom_fake = [](SacnSource* source, uint16_t* universes, size_t universes_size) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universes, nullptr);
    EXPECT_EQ(universes_size, 0u);
    return kTestReturnSize;
  };

  sacn_source_t handle = SACN_SOURCE_INVALID;
  EXPECT_EQ(sacn_source_create(&source_config, &handle), kEtcPalErrOk);
  EXPECT_EQ(sacn_source_add_universe(handle, &universe_config, kTestNetints, NUM_TEST_NETINTS), kEtcPalErrOk);
  unsigned int previous_lock_count = sacn_lock_fake.call_count;
  EXPECT_EQ(sacn_source_get_universes(handle, nullptr, 0u), kTestReturnSize);
  EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);
  EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);
  EXPECT_EQ(get_source_universes_fake.call_count, 1u);
}
