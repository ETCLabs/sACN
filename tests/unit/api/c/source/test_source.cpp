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

#define VERIFY_LOCKING(function_call)                                  \
  do                                                                   \
  {                                                                    \
    unsigned int previous_lock_count = sacn_lock_fake.call_count;      \
    function_call;                                                     \
    EXPECT_NE(sacn_lock_fake.call_count, previous_lock_count);         \
    EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count); \
  } while (0)
#define VERIFY_NO_LOCKING(function_call)                               \
  do                                                                   \
  {                                                                    \
    unsigned int previous_lock_count = sacn_lock_fake.call_count;      \
    function_call;                                                     \
    EXPECT_EQ(sacn_lock_fake.call_count, previous_lock_count);         \
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
#define VERIFY_NO_LOCKING_AND_RETURN_VALUE(function_call, expected_return_value) \
  do                                                                             \
  {                                                                              \
    unsigned int previous_lock_count = sacn_lock_fake.call_count;                \
    EXPECT_EQ(function_call, expected_return_value);                             \
    EXPECT_EQ(sacn_lock_fake.call_count, previous_lock_count);                   \
    EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);           \
  } while (0)

static const etcpal::Uuid kTestLocalCid = etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22");

static constexpr char kTestLocalName[] = "Test Source";
static constexpr char kTestLocalName2[] = "Test Source 2";
static constexpr char kTestLocalNameTooLong[] = "Test Source Name Too Long Test Source Name Too Long Test Source N";

static constexpr sacn_source_t kTestHandle = 123;
static constexpr sacn_source_t kTestHandle2 = 456;

static constexpr uint16_t kTestUniverse = 456u;
static constexpr uint16_t kTestUniverse2 = 789u;
static constexpr uint16_t kTestUniverse3 = 321u;

static constexpr uint8_t kTestPriority = 77u;
static constexpr uint8_t kTestInvalidPriority = 201u;

static constexpr bool kTestPreviewFlag = true;
static constexpr uint8_t kTestStartCode = 0x12u;
static constexpr size_t kTestReturnSize = 1234u;
static constexpr int kTestReturnInt = 5678;

static const std::vector<uint8_t> kTestBuffer = {
    0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu,
};
static const std::vector<uint8_t> kTestBuffer2 = {
    0x0Du, 0x0Eu, 0x0Fu, 0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u, 0x19u, 0x1Au,
};

static const std::vector<EtcPalIpAddr> kTestRemoteAddrs = {
    etcpal::IpAddr::FromString("10.101.1.1").get(), etcpal::IpAddr::FromString("10.101.1.2").get(),
    etcpal::IpAddr::FromString("10.101.1.3").get(), etcpal::IpAddr::FromString("10.101.1.4").get()};
static const std::vector<EtcPalIpAddr> kTestRemoteAddrsWithInvalid = {
    etcpal::IpAddr::FromString("10.101.1.1").get(), etcpal::IpAddr::FromString("10.101.1.2").get(),
    etcpal::IpAddr().get(), etcpal::IpAddr::FromString("10.101.1.4").get()};

static std::vector<SacnMcastInterface> kTestNetints = {{{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}};

static const std::vector<SacnSourceUniverseNetintList> kTestNetintLists = {
    {kTestHandle, kTestUniverse, kTestNetints.data(), kTestNetints.size()},
    {kTestHandle, kTestUniverse2, kTestNetints.data(), kTestNetints.size()},
    {kTestHandle2, kTestUniverse, kTestNetints.data(), kTestNetints.size()},
    {kTestHandle2, kTestUniverse2, kTestNetints.data(), kTestNetints.size()}};
static constexpr size_t kTestNetintListsNumSources = 2u;
static constexpr size_t kTestNetintListsNumUniverses = 2u;

static const std::vector<SacnSourceUniverseNetintList> kTestInvalidNetintLists1 = {
    {kTestHandle, kTestUniverse, kTestNetints.data(), kTestNetints.size()},
    {kTestHandle, kTestUniverse2, kTestNetints.data(), kTestNetints.size()}};
static const std::vector<SacnSourceUniverseNetintList> kTestInvalidNetintLists2 = {
    {kTestHandle, kTestUniverse, kTestNetints.data(), kTestNetints.size()},
    {kTestHandle2, kTestUniverse2, kTestNetints.data(), kTestNetints.size()}};
static const std::vector<SacnSourceUniverseNetintList> kTestInvalidNetintLists3 = {
    {kTestHandle, kTestUniverse, kTestNetints.data(), kTestNetints.size()},
    {kTestHandle, kTestUniverse2, kTestNetints.data(), kTestNetints.size()},
    {kTestHandle2, kTestUniverse, kTestNetints.data(), kTestNetints.size()},
    {kTestHandle2, kTestUniverse2, kTestNetints.data(), kTestNetints.size()},
    {kTestHandle2, kTestUniverse3, kTestNetints.data(), kTestNetints.size()}};

static unsigned int current_netint_list_index = 0u;

class TestSource : public ::testing::Test
{
protected:
  void SetUp() override
  {
    ASSERT_EQ(kTestNetintLists.size(), kTestNetintListsNumSources * kTestNetintListsNumUniverses);

    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_sockets_reset_all_fakes();
    sacn_source_state_reset_all_fakes();

    sacn_initialize_source_netints_fake.custom_fake = [](SacnInternalNetintArray* source_netints,
                                                         SacnMcastInterface* app_netints, size_t num_app_netints) {
#if SACN_DYNAMIC_MEM
      source_netints->netints = (EtcPalMcastNetintId*)calloc(kTestNetints.size(), sizeof(EtcPalMcastNetintId));
      source_netints->netints_capacity = kTestNetints.size();
#endif
      source_netints->num_netints = kTestNetints.size();

      for (size_t i = 0u; i < num_app_netints; ++i)
      {
        EXPECT_EQ(app_netints[i].iface.index, kTestNetints[i].iface.index);
        EXPECT_EQ(app_netints[i].iface.ip_type, kTestNetints[i].iface.ip_type);
        EXPECT_EQ(app_netints[i].status, kTestNetints[i].status);
        source_netints->netints[i] = app_netints[i].iface;
      }

      return kEtcPalErrOk;
    };

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
    source_config.name = kTestLocalName;

    get_next_source_handle_fake.return_val = source_handle;

    sacn_source_t handle = SACN_SOURCE_INVALID;
    EXPECT_EQ(sacn_source_create(&source_config, &handle), kEtcPalErrOk);
  }

  void SetUpSourceAndUniverse(sacn_source_t source_handle, uint16_t universe_id)
  {
    SetUpSource(source_handle);

    SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
    universe_config.universe = universe_id;

    EXPECT_EQ(sacn_source_add_universe(source_handle, &universe_config, kTestNetints.data(), kTestNetints.size()),
              kEtcPalErrOk);
  }

  void SetUpSourcesAndUniverses(const std::vector<SacnSourceUniverseNetintList>& netint_lists, size_t num_netint_lists)
  {
    for (size_t i = 0u; i < num_netint_lists; ++i)
    {
      if (!GetSource(netint_lists[i].handle))
      {
        SacnSourceConfig source_config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
        source_config.cid = kTestLocalCid.get();
        source_config.name = kTestLocalName;

        get_next_source_handle_fake.return_val = netint_lists[i].handle;

        sacn_source_t handle = SACN_SOURCE_INVALID;
        sacn_source_create(&source_config, &handle);
      }

      SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
      universe_config.universe = netint_lists[i].universe;

      EXPECT_EQ(sacn_source_add_universe(netint_lists[i].handle, &universe_config, netint_lists[i].netints,
                                         netint_lists[i].num_netints),
                kEtcPalErrOk);
    }
  }

  SacnSource* GetSource(sacn_source_t handle)
  {
    SacnSource* state = nullptr;
    lookup_source(handle, &state);
    return state;
  }

  SacnSourceUniverse* GetUniverse(sacn_source_t source, uint16_t universe)
  {
    SacnSource* source_state = nullptr;
    SacnSourceUniverse* universe_state = nullptr;
    lookup_source_and_universe(source, universe, &source_state, &universe_state);
    return universe_state;
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

TEST_F(TestSource, SourceConfigInitHandlesNull)
{
  sacn_source_config_init(nullptr);
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

TEST_F(TestSource, SourceUniverseConfigInitHandlesNull)
{
  sacn_source_universe_config_init(nullptr);
}

TEST_F(TestSource, ThreadedSourceCreateWorks)
{
  SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  config.cid = kTestLocalCid.get();
  config.name = kTestLocalName;
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
  config.name = kTestLocalName;
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

TEST_F(TestSource, SourceCreateErrInvalidWorks)
{
  SacnSourceConfig valid_config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  valid_config.cid = kTestLocalCid.get();
  valid_config.name = kTestLocalName;
  SacnSourceConfig null_cid_config = valid_config;
  null_cid_config.cid = kEtcPalNullUuid;
  SacnSourceConfig null_name_config = valid_config;
  null_name_config.name = nullptr;
  SacnSourceConfig lengthy_name_config = valid_config;
  lengthy_name_config.name = kTestLocalNameTooLong;
  SacnSourceConfig zero_keep_alive_config = valid_config;
  zero_keep_alive_config.keep_alive_interval = 0;
  SacnSourceConfig negative_keep_alive_config = valid_config;
  negative_keep_alive_config.keep_alive_interval = -100;

  sacn_source_t handle = SACN_SOURCE_INVALID;

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(nullptr, &handle), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&null_cid_config, &handle), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&null_name_config, &handle), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&lengthy_name_config, &handle), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&zero_keep_alive_config, &handle), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&negative_keep_alive_config, &handle), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&valid_config, nullptr), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&valid_config, &handle), kEtcPalErrOk);
}

TEST_F(TestSource, SourceCreateErrNotInitWorks)
{
  SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  config.cid = kTestLocalCid.get();
  config.name = kTestLocalName;
  sacn_source_t handle = SACN_SOURCE_INVALID;

  sacn_initialized_fake.return_val = false;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&config, &handle), kEtcPalErrNotInit);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&config, &handle), kEtcPalErrOk);
}

#if !SACN_DYNAMIC_MEM
TEST_F(TestSource, SourceCreateErrNoMemWorks)
{
  SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  config.cid = kTestLocalCid.get();
  config.name = kTestLocalName;
  sacn_source_t handle = SACN_SOURCE_INVALID;

  for (int i = 0; i < SACN_SOURCE_MAX_SOURCES; ++i)
  {
    get_next_source_handle_fake.return_val = i;
    VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&config, &handle), kEtcPalErrOk);
  }

  get_next_source_handle_fake.return_val = SACN_SOURCE_MAX_SOURCES;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&config, &handle), kEtcPalErrNoMem);
}
#endif

TEST_F(TestSource, SourceCreateReturnsThreadError)
{
  SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
  config.cid = kTestLocalCid.get();
  config.name = kTestLocalName;
  sacn_source_t handle = SACN_SOURCE_INVALID;

  initialize_source_thread_fake.return_val = kEtcPalErrSys;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&config, &handle), kEtcPalErrSys);
  initialize_source_thread_fake.return_val = kEtcPalErrOk;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_create(&config, &handle), kEtcPalErrOk);
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

TEST_F(TestSource, SourceDestroyHandlesNotInit)
{
  SetUpSource(kTestHandle);

  sacn_initialized_fake.return_val = false;
  VERIFY_NO_LOCKING(sacn_source_destroy(kTestHandle));
  EXPECT_EQ(set_source_terminating_fake.call_count, 0u);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING(sacn_source_destroy(kTestHandle));
  EXPECT_EQ(set_source_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceDestroyHandlesInvalidHandle)
{
  SetUpSource(kTestHandle);

  VERIFY_NO_LOCKING(sacn_source_destroy(SACN_SOURCE_INVALID));
  EXPECT_EQ(set_source_terminating_fake.call_count, 0u);
  VERIFY_LOCKING(sacn_source_destroy(kTestHandle));
  EXPECT_EQ(set_source_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceDestroyHandlesNotFound)
{
  VERIFY_LOCKING(sacn_source_destroy(kTestHandle));
  EXPECT_EQ(set_source_terminating_fake.call_count, 0u);
  SetUpSource(kTestHandle);
  VERIFY_LOCKING(sacn_source_destroy(kTestHandle));
  EXPECT_EQ(set_source_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceDestroyHandlesAlreadyTerminating)
{
  SetUpSource(kTestHandle);
  GetSource(kTestHandle)->terminating = true;
  VERIFY_LOCKING(sacn_source_destroy(kTestHandle));
  EXPECT_EQ(set_source_terminating_fake.call_count, 0u);
  GetSource(kTestHandle)->terminating = false;
  VERIFY_LOCKING(sacn_source_destroy(kTestHandle));
  EXPECT_EQ(set_source_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceChangeNameWorks)
{
  SetUpSource(kTestHandle);

  set_source_name_fake.custom_fake = [](SacnSource* source, const char* new_name) {
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(strcmp(new_name, kTestLocalName2), 0);
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, kTestLocalName2), kEtcPalErrOk);
  EXPECT_EQ(set_source_name_fake.call_count, 1u);
}

TEST_F(TestSource, SourceChangeNameErrInvalidWorks)
{
  SetUpSource(kTestHandle);

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(SACN_SOURCE_INVALID, kTestLocalName2), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, nullptr), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, kTestLocalNameTooLong), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, nullptr), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, kTestLocalName2), kEtcPalErrOk);
}

TEST_F(TestSource, SourceChangeNameErrNotInitWorks)
{
  SetUpSource(kTestHandle);
  sacn_initialized_fake.return_val = false;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, kTestLocalName2), kEtcPalErrNotInit);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, kTestLocalName2), kEtcPalErrOk);
}

TEST_F(TestSource, SourceChangeNameErrNotFoundWorks)
{
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, kTestLocalName2), kEtcPalErrNotFound);
  SetUpSource(kTestHandle);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, kTestLocalName2), kEtcPalErrOk);
  GetSource(kTestHandle)->terminating = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_name(kTestHandle, kTestLocalName2), kEtcPalErrNotFound);
}

TEST_F(TestSource, SourceAddUniverseWorks)
{
  SetUpSource(kTestHandle);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()), kEtcPalErrOk);

  SacnSource* source = nullptr;
  SacnSourceUniverse* universe = nullptr;
  EXPECT_EQ(lookup_source_and_universe(kTestHandle, kTestUniverse, &source, &universe), kEtcPalErrOk);
  if (source)
  {
    EXPECT_EQ(source->num_netints, (size_t)kTestNetints.size());
    for (size_t i = 0; (i < kTestNetints.size()) && (i < source->num_netints); ++i)
    {
      EXPECT_EQ(source->netints[i].id.index, kTestNetints[i].iface.index);
      EXPECT_EQ(source->netints[i].id.ip_type, kTestNetints[i].iface.ip_type);
    }
  }
}

TEST_F(TestSource, SourceAddUniverseErrNoNetintsWorks)
{
  SetUpSource(kTestHandle);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_initialize_source_netints_fake.custom_fake = [](SacnInternalNetintArray*, SacnMcastInterface*, size_t) {
    return kEtcPalErrNoNetints;
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrNoNetints);
}

TEST_F(TestSource, SourceAddUniverseErrInvalidWorks)
{
  SetUpSource(kTestHandle);

  SacnSourceUniverseConfig valid_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  valid_config.universe = kTestUniverse;
  SacnSourceUniverseConfig invalid_universe_config_1 = valid_config;
  invalid_universe_config_1.universe = 0u;
  SacnSourceUniverseConfig invalid_universe_config_2 = valid_config;
  invalid_universe_config_2.universe = 64000u;
  SacnSourceUniverseConfig invalid_sync_universe_config = valid_config;
  invalid_sync_universe_config.sync_universe = 64000u;
  SacnSourceUniverseConfig valid_sync_universe_config_1 = valid_config;
  valid_sync_universe_config_1.universe = kTestUniverse + 1u;
  valid_sync_universe_config_1.sync_universe = 63339u;
  SacnSourceUniverseConfig valid_sync_universe_config_2 = valid_config;
  valid_sync_universe_config_2.universe = kTestUniverse + 2u;
  valid_sync_universe_config_2.sync_universe = 0u;
  SacnSourceUniverseConfig invalid_unicast_dests_config_1 = valid_config;
  invalid_unicast_dests_config_1.num_unicast_destinations = 1u;
  SacnSourceUniverseConfig invalid_unicast_dests_config_2 = valid_config;
  invalid_unicast_dests_config_2.num_unicast_destinations = kTestRemoteAddrsWithInvalid.size();
  invalid_unicast_dests_config_2.unicast_destinations = kTestRemoteAddrsWithInvalid.data();
  SacnSourceUniverseConfig valid_unicast_dests_config_1 = valid_config;
  valid_unicast_dests_config_1.universe = kTestUniverse + 3u;
  valid_unicast_dests_config_1.num_unicast_destinations = 0u;
  SacnSourceUniverseConfig valid_unicast_dests_config_2 = valid_config;
  valid_unicast_dests_config_2.universe = kTestUniverse + 4u;
  valid_unicast_dests_config_2.num_unicast_destinations = kTestRemoteAddrs.size();
  valid_unicast_dests_config_2.unicast_destinations = kTestRemoteAddrs.data();

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(SACN_SOURCE_INVALID, &valid_config, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, nullptr, kTestNetints.data(), kTestNetints.size()), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &valid_config, kTestNetints.data(), kTestNetints.size()), kEtcPalErrOk);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &invalid_universe_config_1, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &invalid_universe_config_2, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &invalid_sync_universe_config, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &valid_sync_universe_config_1, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrOk);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &valid_sync_universe_config_2, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrOk);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &invalid_unicast_dests_config_1, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &invalid_unicast_dests_config_2, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &valid_unicast_dests_config_1, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrOk);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &valid_unicast_dests_config_2, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrOk);
}

TEST_F(TestSource, SourceAddUniverseErrNotInitWorks)
{
  SetUpSource(kTestHandle);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_initialized_fake.return_val = false;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrNotInit);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()), kEtcPalErrOk);
}

TEST_F(TestSource, SourceAddUniverseErrExistsWorks)
{
  SetUpSource(kTestHandle);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()), kEtcPalErrOk);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrExists);
}

TEST_F(TestSource, SourceAddUniverseErrNotFoundWorks)
{
  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrNotFound);
  SetUpSource(kTestHandle);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()), kEtcPalErrOk);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle + 1, &universe_config, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrNotFound);
  GetSource(kTestHandle)->terminating = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrNotFound);
}

#if !SACN_DYNAMIC_MEM
TEST_F(TestSource, SourceAddUniverseErrNoMemWorks)
{
  SetUpSource(kTestHandle);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  for (int i = 0; i < SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE; ++i)
  {
    VERIFY_LOCKING_AND_RETURN_VALUE(
        sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()),
        kEtcPalErrOk);
    ++universe_config.universe;
  }

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size()),
      kEtcPalErrNoMem);
}
#endif

TEST_F(TestSource, SourceRemoveUniverseWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  set_universe_terminating_fake.custom_fake = [](SacnSourceUniverse* universe, set_terminating_behavior_t) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
  };

  VERIFY_LOCKING(sacn_source_remove_universe(kTestHandle, kTestUniverse));
  EXPECT_EQ(set_universe_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceRemoveUniverseHandlesNotFound)
{
  SetUpSource(kTestHandle);

  VERIFY_LOCKING(sacn_source_remove_universe(kTestHandle, kTestUniverse));
  EXPECT_EQ(set_universe_terminating_fake.call_count, 0u);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;
  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;
  VERIFY_LOCKING(sacn_source_remove_universe(kTestHandle, kTestUniverse));
  EXPECT_EQ(set_universe_terminating_fake.call_count, 0u);
  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;
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

TEST_F(TestSource, SourceGetUniversesHandlesNotFound)
{
  VERIFY_LOCKING(sacn_source_get_universes(kTestHandle, nullptr, 0u));
  EXPECT_EQ(get_source_universes_fake.call_count, 0u);
  SetUpSource(kTestHandle);
  GetSource(kTestHandle)->terminating = true;
  VERIFY_LOCKING(sacn_source_get_universes(kTestHandle, nullptr, 0u));
  EXPECT_EQ(get_source_universes_fake.call_count, 0u);
  GetSource(kTestHandle)->terminating = false;
  VERIFY_LOCKING(sacn_source_get_universes(kTestHandle, nullptr, 0u));
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

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrOk);

  SacnSource* source_state = nullptr;
  SacnSourceUniverse* universe_state = nullptr;
  SacnUnicastDestination* unicast_dest = nullptr;
  EXPECT_EQ(lookup_source_and_universe(kTestHandle, kTestUniverse, &source_state, &universe_state), kEtcPalErrOk);
  EXPECT_EQ(lookup_unicast_dest(universe_state, &kTestRemoteAddrs[0], &unicast_dest), kEtcPalErrOk);

  EXPECT_EQ(reset_transmission_suppression_fake.call_count, 1u);
}

TEST_F(TestSource, SourceAddUnicastDestinationErrInvalidWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_unicast_destination(SACN_SOURCE_INVALID, kTestUniverse, &kTestRemoteAddrs[0]), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, 0u, &kTestRemoteAddrs[0]),
                                  kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, 64000u, &kTestRemoteAddrs[0]),
                                  kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, nullptr),
                                  kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &etcpal::IpAddr().get()), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrOk);
}

TEST_F(TestSource, SourceAddUnicastDestinationErrNotInitWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);
  sacn_initialized_fake.return_val = false;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrNotInit);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrOk);
}

TEST_F(TestSource, SourceAddUnicastDestinationErrNotFoundWorks)
{
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrNotFound);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrNotFound);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrNotFound);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrOk);
}

TEST_F(TestSource, SourceAddUnicastDestinationErrExistsWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrOk);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]),
                                  kEtcPalErrExists);
}

#if !SACN_DYNAMIC_MEM
TEST_F(TestSource, SourceAddUnicastDestinationErrNoMemWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  EtcPalIpAddr addr = kTestRemoteAddrs[0];
  for (int i = 0; i < SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE; ++i)
  {
    VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &addr),
                                    kEtcPalErrOk);
    ++addr.addr.v4;
  }

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &addr),
                                  kEtcPalErrNoMem);
}
#endif

TEST_F(TestSource, SourceRemoveUnicastDestinationWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  set_unicast_dest_terminating_fake.custom_fake = [](SacnUnicastDestination* dest, set_terminating_behavior_t) {
    EXPECT_EQ(etcpal_ip_cmp(&dest->dest_addr, &kTestRemoteAddrs[0]), 0);
  };

  EXPECT_EQ(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]), kEtcPalErrOk);
  VERIFY_LOCKING(sacn_source_remove_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]));
  EXPECT_EQ(set_unicast_dest_terminating_fake.call_count, 1u);
}

TEST_F(TestSource, SourceRemoveUnicastDestinationHandlesInvalid)
{
  VERIFY_NO_LOCKING(sacn_source_remove_unicast_destination(kTestHandle, kTestUniverse, nullptr));
  EXPECT_EQ(set_unicast_dest_terminating_fake.call_count, 0u);
}

TEST_F(TestSource, SourceRemoveUnicastDestinationHandlesNotFound)
{
  VERIFY_LOCKING(sacn_source_remove_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]));
  EXPECT_EQ(set_unicast_dest_terminating_fake.call_count, 0u);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING(sacn_source_remove_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]));
  EXPECT_EQ(set_unicast_dest_terminating_fake.call_count, 0u);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  VERIFY_LOCKING(sacn_source_remove_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]));
  EXPECT_EQ(set_unicast_dest_terminating_fake.call_count, 0u);

  EXPECT_EQ(sacn_source_add_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]), kEtcPalErrOk);

  SacnUnicastDestination* dest = nullptr;
  lookup_unicast_dest(GetUniverse(kTestHandle, kTestUniverse), &kTestRemoteAddrs[0], &dest);

  dest->termination_state = kTerminatingAndRemoving;
  VERIFY_LOCKING(sacn_source_remove_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]));
  EXPECT_EQ(set_unicast_dest_terminating_fake.call_count, 0u);

  dest->termination_state = kNotTerminating;
  VERIFY_LOCKING(sacn_source_remove_unicast_destination(kTestHandle, kTestUniverse, &kTestRemoteAddrs[0]));
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

TEST_F(TestSource, SourceGetUnicastDestinationsHandlesNotFound)
{
  VERIFY_LOCKING(sacn_source_get_unicast_destinations(kTestHandle, kTestUniverse, nullptr, 0u));
  EXPECT_EQ(get_source_unicast_dests_fake.call_count, 0u);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING(sacn_source_get_unicast_destinations(kTestHandle, kTestUniverse, nullptr, 0u));
  EXPECT_EQ(get_source_unicast_dests_fake.call_count, 0u);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;
  VERIFY_LOCKING(sacn_source_get_unicast_destinations(kTestHandle, kTestUniverse, nullptr, 0u));
  EXPECT_EQ(get_source_unicast_dests_fake.call_count, 0u);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;
  VERIFY_LOCKING(sacn_source_get_unicast_destinations(kTestHandle, kTestUniverse, nullptr, 0u));
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

TEST_F(TestSource, SourceChangePriorityErrInvalidWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(SACN_SOURCE_INVALID, kTestUniverse, kTestPriority),
                                  kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, 0u, kTestPriority), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, 64000u, kTestPriority), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, kTestUniverse, kTestInvalidPriority),
                                  kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, kTestUniverse, kTestPriority), kEtcPalErrOk);
}

TEST_F(TestSource, SourceChangePriorityErrNotInitWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  sacn_initialized_fake.return_val = false;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, kTestUniverse, kTestPriority),
                                  kEtcPalErrNotInit);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, kTestUniverse, kTestPriority), kEtcPalErrOk);
}

TEST_F(TestSource, SourceChangePriorityErrNotFoundWorks)
{
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, kTestUniverse, kTestPriority),
                                  kEtcPalErrNotFound);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, kTestUniverse, kTestPriority),
                                  kEtcPalErrNotFound);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, kTestUniverse, kTestPriority),
                                  kEtcPalErrNotFound);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_priority(kTestHandle, kTestUniverse, kTestPriority), kEtcPalErrOk);
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

TEST_F(TestSource, SourceChangePreviewFlagErrInvalidWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(SACN_SOURCE_INVALID, kTestUniverse, true),
                                  kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, 0u, true), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, 64000u, true), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, kTestUniverse, true), kEtcPalErrOk);
}

TEST_F(TestSource, SourceChangePreviewFlagErrNotInitWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  sacn_initialized_fake.return_val = false;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, kTestUniverse, true), kEtcPalErrNotInit);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, kTestUniverse, true), kEtcPalErrOk);
}

TEST_F(TestSource, SourceChangePreviewFlagErrNotFoundWorks)
{
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, kTestUniverse, true),
                                  kEtcPalErrNotFound);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, kTestUniverse, true),
                                  kEtcPalErrNotFound);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, kTestUniverse, true),
                                  kEtcPalErrNotFound);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_change_preview_flag(kTestHandle, kTestUniverse, true), kEtcPalErrOk);
}

TEST_F(TestSource, SourceSendNowWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  send_universe_multicast_fake.custom_fake = [](const SacnSource* source, SacnSourceUniverse* universe,
                                                const uint8_t* send_buf) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universe->universe_id, kTestUniverse);
    EXPECT_EQ(send_buf[SACN_DATA_HEADER_SIZE - 1], kTestStartCode);
    EXPECT_EQ(memcmp(&send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer.data(), kTestBuffer.size()), 0);
  };
  send_universe_unicast_fake.custom_fake = [](const SacnSource* source, SacnSourceUniverse* universe,
                                              const uint8_t* send_buf) {
    EXPECT_EQ(source->handle, kTestHandle);
    EXPECT_EQ(universe->universe_id, kTestUniverse);
    EXPECT_EQ(send_buf[SACN_DATA_HEADER_SIZE - 1], kTestStartCode);
    EXPECT_EQ(memcmp(&send_buf[SACN_DATA_HEADER_SIZE], kTestBuffer.data(), kTestBuffer.size()), 0);
  };
  increment_sequence_number_fake.custom_fake = [](SacnSourceUniverse* universe) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
  };

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrOk);

  EXPECT_EQ(send_universe_multicast_fake.call_count, 1u);
  EXPECT_EQ(send_universe_unicast_fake.call_count, 1u);
  EXPECT_EQ(increment_sequence_number_fake.call_count, 1u);
}

TEST_F(TestSource, SourceSendNowErrInvalidWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(SACN_SOURCE_INVALID, kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, 0u, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, 64000u, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), (DMX_ADDRESS_COUNT + 1u)),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, nullptr, kTestBuffer.size()), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), 0u), kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrOk);
}

TEST_F(TestSource, SourceSendNowErrNotInitWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);
  sacn_initialized_fake.return_val = false;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrNotInit);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrOk);
}

TEST_F(TestSource, SourceSendNowErrNotFoundWorks)
{
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrNotFound);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrNotFound);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrNotFound);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;

  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_send_now(kTestHandle, kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()),
      kEtcPalErrOk);
}

TEST_F(TestSource, SourceUpdateValuesWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  update_levels_and_or_paps_fake.custom_fake =
      [](SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_levels, size_t new_levels_size,
         const uint8_t* new_priorities, size_t new_priorities_size, force_sync_behavior_t force_sync) {
        EXPECT_EQ(source->handle, kTestHandle);
        EXPECT_EQ(universe->universe_id, kTestUniverse);
        EXPECT_EQ(memcmp(new_levels, kTestBuffer.data(), kTestBuffer.size()), 0);
        EXPECT_EQ(new_levels_size, kTestBuffer.size());
        EXPECT_EQ(new_priorities, nullptr);
        EXPECT_EQ(new_priorities_size, 0u);
        EXPECT_EQ(force_sync, kDisableForceSync);
      };

  VERIFY_LOCKING(sacn_source_update_values(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesHandlesInvalid)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  VERIFY_NO_LOCKING(
      sacn_source_update_values(kTestHandle, kTestUniverse, kTestBuffer.data(), (DMX_ADDRESS_COUNT + 1u)));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);
  VERIFY_LOCKING(sacn_source_update_values(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesHandlesNotFound)
{
  VERIFY_LOCKING(sacn_source_update_values(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING(sacn_source_update_values(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;

  VERIFY_LOCKING(sacn_source_update_values(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;

  VERIFY_LOCKING(sacn_source_update_values(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
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
        EXPECT_EQ(memcmp(new_levels, kTestBuffer.data(), kTestBuffer.size()), 0);
        EXPECT_EQ(new_levels_size, kTestBuffer.size());
        if (new_priorities)
        {
          EXPECT_EQ(memcmp(new_priorities, kTestBuffer2.data(), kTestBuffer2.size()), 0);
          EXPECT_EQ(new_priorities_size, kTestBuffer2.size());
        }
        EXPECT_EQ(force_sync, kDisableForceSync);
      };

  VERIFY_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(),
                                                   kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
  EXPECT_EQ(disable_pap_data_fake.call_count, 0u);

  // Check disabling PAP as well
  disable_pap_data_fake.custom_fake = [](SacnSourceUniverse* universe) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
  };
  VERIFY_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(),
                                                   nullptr, 0u));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 2u);
  EXPECT_EQ(disable_pap_data_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndPapHandlesInvalid)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  VERIFY_NO_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer.data(),
                                                      (DMX_ADDRESS_COUNT + 1u), kTestBuffer2.data(),
                                                      kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);
  VERIFY_NO_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer.data(),
                                                      kTestBuffer.size(), kTestBuffer2.data(),
                                                      (DMX_ADDRESS_COUNT + 1u)));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);
  VERIFY_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(),
                                                   kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndPapHandlesNotFound)
{
  VERIFY_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(),
                                                   kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(),
                                                   kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;

  VERIFY_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(),
                                                   kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;

  VERIFY_LOCKING(sacn_source_update_values_and_pap(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(),
                                                   kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndForceSyncWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  update_levels_and_or_paps_fake.custom_fake =
      [](SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_levels, size_t new_levels_size,
         const uint8_t* new_priorities, size_t new_priorities_size, force_sync_behavior_t force_sync) {
        EXPECT_EQ(source->handle, kTestHandle);
        EXPECT_EQ(universe->universe_id, kTestUniverse);
        EXPECT_EQ(memcmp(new_levels, kTestBuffer.data(), kTestBuffer.size()), 0);
        EXPECT_EQ(new_levels_size, kTestBuffer.size());
        EXPECT_EQ(new_priorities, nullptr);
        EXPECT_EQ(new_priorities_size, 0u);
        EXPECT_EQ(force_sync, kEnableForceSync);
      };

  VERIFY_LOCKING(
      sacn_source_update_values_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndForceSyncHandlesInvalid)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  VERIFY_NO_LOCKING(sacn_source_update_values_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(),
                                                             (DMX_ADDRESS_COUNT + 1u)));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);
  VERIFY_LOCKING(
      sacn_source_update_values_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndForceSyncHandlesNotFound)
{
  VERIFY_LOCKING(
      sacn_source_update_values_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING(
      sacn_source_update_values_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;

  VERIFY_LOCKING(
      sacn_source_update_values_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;

  VERIFY_LOCKING(
      sacn_source_update_values_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size()));
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
        EXPECT_EQ(memcmp(new_levels, kTestBuffer.data(), kTestBuffer.size()), 0);
        EXPECT_EQ(new_levels_size, kTestBuffer.size());
        if (new_priorities)
        {
          EXPECT_EQ(memcmp(new_priorities, kTestBuffer2.data(), kTestBuffer2.size()), 0);
          EXPECT_EQ(new_priorities_size, kTestBuffer2.size());
        }
        EXPECT_EQ(force_sync, kEnableForceSync);
      };

  VERIFY_LOCKING(sacn_source_update_values_and_pap_and_force_sync(
      kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
  EXPECT_EQ(disable_pap_data_fake.call_count, 0u);

  // Check disabling PAP as well
  disable_pap_data_fake.custom_fake = [](SacnSourceUniverse* universe) {
    EXPECT_EQ(universe->universe_id, kTestUniverse);
  };
  VERIFY_LOCKING(sacn_source_update_values_and_pap_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(),
                                                                  kTestBuffer.size(), nullptr, 0u));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 2u);
  EXPECT_EQ(disable_pap_data_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndPapAndForceSyncHandlesInvalid)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  VERIFY_NO_LOCKING(sacn_source_update_values_and_pap_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(),
                                                                     (DMX_ADDRESS_COUNT + 1u), kTestBuffer2.data(),
                                                                     kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);
  VERIFY_NO_LOCKING(sacn_source_update_values_and_pap_and_force_sync(kTestHandle, kTestUniverse, kTestBuffer.data(),
                                                                     kTestBuffer.size(), kTestBuffer2.data(),
                                                                     (DMX_ADDRESS_COUNT + 1u)));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);
  VERIFY_LOCKING(sacn_source_update_values_and_pap_and_force_sync(
      kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
}

TEST_F(TestSource, SourceUpdateValuesAndPapAndForceSyncHandlesNotFound)
{
  VERIFY_LOCKING(sacn_source_update_values_and_pap_and_force_sync(
      kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING(sacn_source_update_values_and_pap_and_force_sync(
      kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;

  VERIFY_LOCKING(sacn_source_update_values_and_pap_and_force_sync(
      kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 0u);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;

  VERIFY_LOCKING(sacn_source_update_values_and_pap_and_force_sync(
      kTestHandle, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer2.data(), kTestBuffer2.size()));
  EXPECT_EQ(update_levels_and_or_paps_fake.call_count, 1u);
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

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_reset_networking(kTestNetints.data(), kTestNetints.size()), kEtcPalErrOk);

  EXPECT_EQ(sacn_sockets_reset_source_fake.call_count, 1u);
  EXPECT_EQ(clear_source_netints_fake.call_count, 1u);
  EXPECT_EQ(reset_source_universe_networking_fake.call_count, 1u);
}

TEST_F(TestSource, SourceResetNetworkingErrNoNetintsWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);

  reset_source_universe_networking_fake.return_val = kEtcPalErrNoNetints;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_reset_networking(kTestNetints.data(), kTestNetints.size()),
                                  kEtcPalErrNoNetints);
  reset_source_universe_networking_fake.return_val = kEtcPalErrOk;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_reset_networking(kTestNetints.data(), kTestNetints.size()), kEtcPalErrOk);
}

TEST_F(TestSource, SourceResetNetworkingErrNotInitWorks)
{
  SetUpSourceAndUniverse(kTestHandle, kTestUniverse);
  sacn_initialized_fake.return_val = false;
  VERIFY_NO_LOCKING_AND_RETURN_VALUE(sacn_source_reset_networking(kTestNetints.data(), kTestNetints.size()),
                                     kEtcPalErrNotInit);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_reset_networking(kTestNetints.data(), kTestNetints.size()), kEtcPalErrOk);
}

TEST_F(TestSource, SourceResetNetworkingPerUniverseWorks)
{
  SetUpSourcesAndUniverses(kTestNetintLists, kTestNetintLists.size());

  clear_source_netints_fake.custom_fake = [](SacnSource* source) {
    EXPECT_EQ(source->handle, kTestNetintLists[current_netint_list_index].handle);

    if (current_netint_list_index >= (kTestNetintLists.size() - kTestNetintListsNumUniverses))
      current_netint_list_index = 0u;
    else
      current_netint_list_index += kTestNetintListsNumUniverses;
  };
  reset_source_universe_networking_fake.custom_fake = [](SacnSource* source, SacnSourceUniverse* universe,
                                                         SacnMcastInterface* netints, size_t num_netints) {
    EXPECT_EQ(source->handle, kTestNetintLists[current_netint_list_index].handle);
    EXPECT_EQ(universe->universe_id, kTestNetintLists[current_netint_list_index].universe);
    EXPECT_EQ(num_netints, kTestNetintLists[current_netint_list_index].num_netints);

    for (size_t i = 0u; i < num_netints; ++i)
    {
      EXPECT_EQ(netints[i].iface.index, kTestNetintLists[current_netint_list_index].netints[i].iface.index);
      EXPECT_EQ(netints[i].iface.ip_type, kTestNetintLists[current_netint_list_index].netints[i].iface.ip_type);
      EXPECT_EQ(netints[i].status, kTestNetintLists[current_netint_list_index].netints[i].status);
    }

    ++current_netint_list_index;

    return kEtcPalErrOk;
  };

  current_netint_list_index = 0u;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_reset_networking_per_universe(kTestNetintLists.data(), kTestNetintLists.size()), kEtcPalErrOk);

  EXPECT_EQ(current_netint_list_index, kTestNetintLists.size());
  EXPECT_EQ(sacn_sockets_reset_source_fake.call_count, 1u);
  EXPECT_EQ(clear_source_netints_fake.call_count, kTestNetintListsNumSources);
  EXPECT_EQ(reset_source_universe_networking_fake.call_count, kTestNetintLists.size());
}

TEST_F(TestSource, SourceResetNetworkingPerUniverseErrNoNetintsWorks)
{
  SetUpSourcesAndUniverses(kTestNetintLists, kTestNetintLists.size());

  reset_source_universe_networking_fake.return_val = kEtcPalErrNoNetints;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_reset_networking_per_universe(kTestNetintLists.data(), kTestNetintLists.size()), kEtcPalErrNoNetints);
  reset_source_universe_networking_fake.return_val = kEtcPalErrOk;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_reset_networking_per_universe(kTestNetintLists.data(), kTestNetintLists.size()), kEtcPalErrOk);
}

TEST_F(TestSource, SourceResetNetworkingPerUniverseErrInvalidWorks)
{
  SetUpSourcesAndUniverses(kTestNetintLists, kTestNetintLists.size());

  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_reset_networking_per_universe(nullptr, kTestNetintLists.size()),
                                  kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(sacn_source_reset_networking_per_universe(kTestNetintLists.data(), 0u),
                                  kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_reset_networking_per_universe(kTestInvalidNetintLists1.data(), kTestInvalidNetintLists1.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_reset_networking_per_universe(kTestInvalidNetintLists2.data(), kTestInvalidNetintLists2.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_reset_networking_per_universe(kTestInvalidNetintLists3.data(), kTestInvalidNetintLists3.size()),
      kEtcPalErrInvalid);
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_reset_networking_per_universe(kTestNetintLists.data(), kTestNetintLists.size()), kEtcPalErrOk);
}

TEST_F(TestSource, SourceResetNetworkingPerUniverseErrNotInitWorks)
{
  SetUpSourcesAndUniverses(kTestNetintLists, kTestNetintLists.size());

  sacn_initialized_fake.return_val = false;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_reset_networking_per_universe(kTestNetintLists.data(), kTestNetintLists.size()), kEtcPalErrNotInit);
  sacn_initialized_fake.return_val = true;
  VERIFY_LOCKING_AND_RETURN_VALUE(
      sacn_source_reset_networking_per_universe(kTestNetintLists.data(), kTestNetintLists.size()), kEtcPalErrOk);
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

TEST_F(TestSource, SourceGetNetintsHandlesNotFound)
{
  VERIFY_LOCKING(sacn_source_get_network_interfaces(kTestHandle, kTestUniverse, nullptr, 0u));
  EXPECT_EQ(get_source_universe_netints_fake.call_count, 0u);

  SetUpSource(kTestHandle);

  VERIFY_LOCKING(sacn_source_get_network_interfaces(kTestHandle, kTestUniverse, nullptr, 0u));
  EXPECT_EQ(get_source_universe_netints_fake.call_count, 0u);

  SacnSourceUniverseConfig universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
  universe_config.universe = kTestUniverse;

  sacn_source_add_universe(kTestHandle, &universe_config, kTestNetints.data(), kTestNetints.size());

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kTerminatingAndRemoving;
  VERIFY_LOCKING(sacn_source_get_network_interfaces(kTestHandle, kTestUniverse, nullptr, 0u));
  EXPECT_EQ(get_source_universe_netints_fake.call_count, 0u);

  GetUniverse(kTestHandle, kTestUniverse)->termination_state = kNotTerminating;
  VERIFY_LOCKING(sacn_source_get_network_interfaces(kTestHandle, kTestUniverse, nullptr, 0u));
  EXPECT_EQ(get_source_universe_netints_fake.call_count, 1u);
}
