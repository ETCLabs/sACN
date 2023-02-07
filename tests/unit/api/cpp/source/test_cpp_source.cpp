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

#include "sacn/cpp/source.h"

#include <limits>
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/source_loss.h"
#include "sacn_mock/private/sockets.h"
#include "sacn_mock/private/source.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "gtest/gtest.h"

#if SACN_DYNAMIC_MEM
#define TestSource TestCppSourceDynamic
#else
#define TestSource TestCppSourceStatic
#endif

static const etcpal::Uuid kTestLocalCid = etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22");
static const std::string kTestLocalName = "Test Source";
static const std::string kTestLocalName2 = "Test Source 2";
static constexpr uint16_t kTestUniverse = 123u;
static constexpr uint16_t kTestUniverse2 = 456u;
static constexpr uint16_t kTestSyncUniverse = 789u;
static constexpr sacn_source_t kTestHandle = 456;
static constexpr sacn_source_t kTestHandle2 = 654;
static constexpr uint8_t kTestPriority = 77u;
static constexpr bool kTestPreviewFlag = true;
static constexpr uint8_t kTestStartCode = 12u;

static std::vector<SacnMcastInterface> kTestNetints = {
    {{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},  {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
    {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk},  {{kEtcPalIpTypeV4, 4u}, kEtcPalErrOk},
    {{kEtcPalIpTypeV4, 5u}, kEtcPalErrOk},  {{kEtcPalIpTypeV4, 6u}, kEtcPalErrOk},
    {{kEtcPalIpTypeV4, 7u}, kEtcPalErrOk},  {{kEtcPalIpTypeV4, 8u}, kEtcPalErrOk},
    {{kEtcPalIpTypeV4, 9u}, kEtcPalErrOk},  {{kEtcPalIpTypeV4, 10u}, kEtcPalErrOk},
    {{kEtcPalIpTypeV4, 11u}, kEtcPalErrOk}, {{kEtcPalIpTypeV4, 12u}, kEtcPalErrOk},
    {{kEtcPalIpTypeV4, 13u}, kEtcPalErrOk}, {{kEtcPalIpTypeV4, 14u}, kEtcPalErrOk},
    {{kEtcPalIpTypeV4, 15u}, kEtcPalErrOk}};
static std::vector<SacnMcastInterface> kTestNetintsEmpty = {};

static std::vector<sacn::Source::UniverseNetintList> kTestNetintLists = {{kTestHandle, kTestUniverse, kTestNetints},
                                                                         {kTestHandle, kTestUniverse2, kTestNetints},
                                                                         {kTestHandle2, kTestUniverse, kTestNetints},
                                                                         {kTestHandle2, kTestUniverse2, kTestNetints}};

static const std::vector<uint16_t> kTestUniverses = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u};

static const std::vector<etcpal::IpAddr> kTestRemoteAddrs = {
    etcpal::IpAddr::FromString("10.101.1.1"),  etcpal::IpAddr::FromString("10.101.1.2"),
    etcpal::IpAddr::FromString("10.101.1.3"),  etcpal::IpAddr::FromString("10.101.1.4"),
    etcpal::IpAddr::FromString("10.101.1.5"),  etcpal::IpAddr::FromString("10.101.1.6"),
    etcpal::IpAddr::FromString("10.101.1.7"),  etcpal::IpAddr::FromString("10.101.1.8"),
    etcpal::IpAddr::FromString("10.101.1.9"),  etcpal::IpAddr::FromString("10.101.1.10"),
    etcpal::IpAddr::FromString("10.101.1.11"), etcpal::IpAddr::FromString("10.101.1.12"),
    etcpal::IpAddr::FromString("10.101.1.13"), etcpal::IpAddr::FromString("10.101.1.14"),
    etcpal::IpAddr::FromString("10.101.1.15")};

static const std::vector<uint8_t> kTestBuffer = {
    0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu,
};
static const std::vector<uint8_t> kTestBuffer2 = {
    0x0Du, 0x0Eu, 0x0Fu, 0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u, 0x18u, 0x19u, 0x1Au,
};

static std::vector<uint16_t> current_universes;
static std::vector<EtcPalIpAddr> current_dests;
static std::vector<SacnMcastInterface> current_netints;

class TestSource : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_source_loss_reset_all_fakes();
    sacn_sockets_reset_all_fakes();
    sacn_source_reset_all_fakes();

    sacn_source_create_fake.custom_fake = [](const SacnSourceConfig*, sacn_source_t* handle) {
      *handle = kTestHandle;
      return kEtcPalErrOk;
    };

    ASSERT_EQ(sacn_source_mem_init(), kEtcPalErrOk);
  }

  void TearDown() override { sacn_source_mem_deinit(); }
};

TEST_F(TestSource, SettingsConstructorWorks)
{
  sacn::Source::Settings settings(kTestLocalCid, kTestLocalName);
  EXPECT_EQ(ETCPAL_UUID_CMP(&settings.cid.get(), &kTestLocalCid.get()), 0);
  EXPECT_EQ(settings.name, kTestLocalName);
  EXPECT_EQ(settings.universe_count_max, static_cast<size_t>(SACN_SOURCE_INFINITE_UNIVERSES));
  EXPECT_EQ(settings.manually_process_source, false);
  EXPECT_EQ(settings.ip_supported, kSacnIpV4AndIpV6);
  EXPECT_EQ(settings.keep_alive_interval, SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT);
  EXPECT_EQ(settings.pap_keep_alive_interval, SACN_SOURCE_PAP_KEEP_ALIVE_INTERVAL_DEFAULT);
}

TEST_F(TestSource, SettingsIsValidWorks)
{
  sacn::Source::Settings valid_settings(kTestLocalCid, kTestLocalName);
  sacn::Source::Settings invalid_settings_1(etcpal::Uuid(), kTestLocalName);
  sacn::Source::Settings invalid_settings_2;

  EXPECT_EQ(valid_settings.IsValid(), true);
  EXPECT_EQ(invalid_settings_1.IsValid(), false);
  EXPECT_EQ(invalid_settings_2.IsValid(), false);
}

TEST_F(TestSource, UniverseSettingsConstructorWorks)
{
  sacn::Source::UniverseSettings settings(kTestUniverse);
  EXPECT_EQ(settings.universe, kTestUniverse);
  EXPECT_EQ(settings.priority, 100u);
  EXPECT_EQ(settings.send_preview, false);
  EXPECT_EQ(settings.send_unicast_only, false);
  EXPECT_EQ(settings.unicast_destinations.empty(), true);
  EXPECT_EQ(settings.sync_universe, 0u);
}

TEST_F(TestSource, UniverseSettingsIsValidWorks)
{
  sacn::Source::UniverseSettings valid_settings(kTestUniverse);
  sacn::Source::UniverseSettings invalid_settings_1(0u);
  sacn::Source::UniverseSettings invalid_settings_2(64000u);
  sacn::Source::UniverseSettings invalid_settings_3;

  EXPECT_EQ(valid_settings.IsValid(), true);
  EXPECT_EQ(invalid_settings_1.IsValid(), false);
  EXPECT_EQ(invalid_settings_2.IsValid(), false);
  EXPECT_EQ(invalid_settings_3.IsValid(), false);
}

TEST_F(TestSource, UniverseNetintListConstructorWorks)
{
  sacn::Source::UniverseNetintList list(kTestHandle, kTestUniverse);
  EXPECT_EQ(list.handle, kTestHandle);
  EXPECT_EQ(list.universe, kTestUniverse);
  EXPECT_EQ(list.netints.empty(), true);
}

TEST_F(TestSource, StartupWorks)
{
  sacn_source_create_fake.custom_fake = [](const SacnSourceConfig* config, sacn_source_t* handle) {
    EXPECT_EQ(ETCPAL_UUID_CMP(&config->cid, &kTestLocalCid.get()), 0);
    EXPECT_EQ(strcmp(config->name, kTestLocalName.c_str()), 0);
    EXPECT_EQ(config->universe_count_max, static_cast<size_t>(SACN_SOURCE_INFINITE_UNIVERSES));
    EXPECT_EQ(config->manually_process_source, false);
    EXPECT_EQ(config->ip_supported, kSacnIpV4AndIpV6);
    EXPECT_EQ(config->keep_alive_interval, SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT);
    EXPECT_EQ(config->pap_keep_alive_interval, SACN_SOURCE_PAP_KEEP_ALIVE_INTERVAL_DEFAULT);
    EXPECT_NE(handle, nullptr);
    *handle = kTestHandle;
    return kEtcPalErrOk;
  };

  sacn::Source source;
  etcpal::Error result = source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(sacn_source_create_fake.call_count, 1u);
  EXPECT_EQ(source.handle().value(), kTestHandle);
  EXPECT_EQ(result.IsOk(), true);
}

TEST_F(TestSource, ShutdownWorks)
{
  sacn_source_destroy_fake.custom_fake = [](sacn_source_t handle) { EXPECT_EQ(handle, kTestHandle); };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.handle().value(), kTestHandle);
  source.Shutdown();
  EXPECT_EQ(sacn_source_destroy_fake.call_count, 1u);
  EXPECT_EQ(source.handle().value(), SACN_SOURCE_INVALID);
}

TEST_F(TestSource, ChangeNameWorks)
{
  sacn_source_change_name_fake.custom_fake = [](sacn_source_t handle, const char* new_name) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(strcmp(new_name, kTestLocalName2.c_str()), 0);
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.ChangeName(kTestLocalName2).IsOk(), true);
  EXPECT_EQ(sacn_source_change_name_fake.call_count, 1u);
}

TEST_F(TestSource, AddUniverseWorksWithoutNetints)
{
  sacn_source_add_universe_fake.custom_fake = [](sacn_source_t handle, const SacnSourceUniverseConfig* config,
                                                 const SacnNetintConfig* netint_config) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(config->universe, kTestUniverse);
    EXPECT_EQ(config->priority, 100u);
    EXPECT_EQ(config->send_preview, false);
    EXPECT_EQ(config->send_unicast_only, false);
    EXPECT_EQ(config->unicast_destinations, nullptr);
    EXPECT_EQ(config->num_unicast_destinations, 0u);
    EXPECT_EQ(config->sync_universe, 0u);
    if (netint_config)
    {
      EXPECT_EQ(netint_config->netints, nullptr);
      EXPECT_EQ(netint_config->num_netints, 0u);
      EXPECT_FALSE(netint_config->no_netints);
    }
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.AddUniverse(sacn::Source::UniverseSettings(kTestUniverse)).IsOk(), true);
  EXPECT_EQ(sacn_source_add_universe_fake.call_count, 1u);
  EXPECT_EQ(source.AddUniverse(sacn::Source::UniverseSettings(kTestUniverse), kTestNetintsEmpty).IsOk(), true);
  EXPECT_EQ(sacn_source_add_universe_fake.call_count, 2u);
}

TEST_F(TestSource, AddUniverseWorksWithNetints)
{
  sacn_source_add_universe_fake.custom_fake = [](sacn_source_t handle, const SacnSourceUniverseConfig* config,
                                                 const SacnNetintConfig* netint_config) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(config->universe, kTestUniverse);
    EXPECT_EQ(config->priority, 100u);
    EXPECT_EQ(config->send_preview, false);
    EXPECT_EQ(config->send_unicast_only, false);
    EXPECT_EQ(config->unicast_destinations, nullptr);
    EXPECT_EQ(config->num_unicast_destinations, 0u);
    EXPECT_EQ(config->sync_universe, 0u);
    EXPECT_NE(netint_config, nullptr);
    if (netint_config)
    {
      EXPECT_EQ(netint_config->netints, kTestNetints.data());
      EXPECT_EQ(netint_config->num_netints, kTestNetints.size());
    }
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.AddUniverse(sacn::Source::UniverseSettings(kTestUniverse), kTestNetints).IsOk(), true);
  EXPECT_EQ(sacn_source_add_universe_fake.call_count, 1u);
}

TEST_F(TestSource, RemoveUniverseWorks)
{
  sacn_source_remove_universe_fake.custom_fake = [](sacn_source_t handle, uint16_t universe) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  source.RemoveUniverse(kTestUniverse);
  EXPECT_EQ(sacn_source_remove_universe_fake.call_count, 1u);
}

TEST_F(TestSource, GetGrowingUniversesWorks)
{
  sacn_source_get_universes_fake.custom_fake = [](sacn_source_t handle, uint16_t* universes, size_t universes_size) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_NE(universes, nullptr);
    EXPECT_EQ(universes_size, current_universes.size() + 4u);

    for (int i = 0; (i < 5) && (current_universes.size() < kTestUniverses.size()); ++i)
      current_universes.push_back(kTestUniverses[current_universes.size()]);

    if (sacn_source_get_universes_fake.call_count < 4u)
      EXPECT_LT(universes_size, current_universes.size());
    else
      EXPECT_GT(universes_size, current_universes.size());

    for (size_t i = 0; (i < universes_size) && (i < current_universes.size()); ++i)
      universes[i] = current_universes[i];

    return current_universes.size();
  };

  current_universes.clear();

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.GetUniverses(), kTestUniverses);
  EXPECT_EQ(sacn_source_get_universes_fake.call_count, 4u);
}

TEST_F(TestSource, GetUnchangingUniversesWorks)
{
  sacn_source_get_universes_fake.custom_fake = [](sacn_source_t handle, uint16_t* universes, size_t universes_size) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_NE(universes, nullptr);

    if (sacn_source_get_universes_fake.call_count == 1u)
      EXPECT_EQ(universes_size, 4u);
    else
      EXPECT_EQ(universes_size, kTestUniverses.size() + 4u);

    for (size_t i = 0; (i < universes_size) && (i < kTestUniverses.size()); ++i)
      universes[i] = kTestUniverses[i];

    return kTestUniverses.size();
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.GetUniverses(), kTestUniverses);
  EXPECT_EQ(sacn_source_get_universes_fake.call_count, 2u);
}

TEST_F(TestSource, AddUnicastDestinationWorks)
{
  sacn_source_add_unicast_destination_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                            const EtcPalIpAddr* dest) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(etcpal_ip_cmp(dest, &kTestRemoteAddrs[0].get()), 0);
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.AddUnicastDestination(kTestUniverse, kTestRemoteAddrs[0]).IsOk(), true);
  EXPECT_EQ(sacn_source_add_unicast_destination_fake.call_count, 1u);
}

TEST_F(TestSource, RemoveUnicastDestinationWorks)
{
  sacn_source_remove_unicast_destination_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                               const EtcPalIpAddr* dest) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(etcpal_ip_cmp(dest, &kTestRemoteAddrs[0].get()), 0);
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  source.RemoveUnicastDestination(kTestUniverse, kTestRemoteAddrs[0]);
  EXPECT_EQ(sacn_source_remove_unicast_destination_fake.call_count, 1u);
}

TEST_F(TestSource, GetGrowingUnicastDestinationsWorks)
{
  sacn_source_get_unicast_destinations_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                             EtcPalIpAddr* destinations, size_t destinations_size) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_NE(destinations, nullptr);
    EXPECT_EQ(destinations_size, current_dests.size() + 4u);

    for (int i = 0; (i < 5) && (current_dests.size() < kTestRemoteAddrs.size()); ++i)
      current_dests.push_back(kTestRemoteAddrs[current_dests.size()].get());

    if (sacn_source_get_unicast_destinations_fake.call_count < 4u)
      EXPECT_LT(destinations_size, current_dests.size());
    else
      EXPECT_GT(destinations_size, current_dests.size());

    for (size_t i = 0; (i < destinations_size) && (i < current_dests.size()); ++i)
      destinations[i] = current_dests[i];

    return current_dests.size();
  };

  current_dests.clear();

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.GetUnicastDestinations(kTestUniverse), kTestRemoteAddrs);
  EXPECT_EQ(sacn_source_get_unicast_destinations_fake.call_count, 4u);
}

TEST_F(TestSource, GetUnchangingUnicastDestinationsWorks)
{
  sacn_source_get_unicast_destinations_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                             EtcPalIpAddr* destinations, size_t destinations_size) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_NE(destinations, nullptr);

    if (sacn_source_get_unicast_destinations_fake.call_count == 1u)
      EXPECT_EQ(destinations_size, 4u);
    else
      EXPECT_EQ(destinations_size, kTestRemoteAddrs.size() + 4u);

    for (size_t i = 0; (i < destinations_size) && (i < kTestRemoteAddrs.size()); ++i)
      destinations[i] = kTestRemoteAddrs[i].get();

    return kTestRemoteAddrs.size();
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.GetUnicastDestinations(kTestUniverse), kTestRemoteAddrs);
  EXPECT_EQ(sacn_source_get_unicast_destinations_fake.call_count, 2u);
}

TEST_F(TestSource, ChangePriorityWorks)
{
  sacn_source_change_priority_fake.custom_fake = [](sacn_source_t handle, uint16_t universe, uint8_t new_priority) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(new_priority, kTestPriority);
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.ChangePriority(kTestUniverse, kTestPriority).IsOk(), true);
  EXPECT_EQ(sacn_source_change_priority_fake.call_count, 1u);
}

TEST_F(TestSource, ChangePreviewFlagWorks)
{
  sacn_source_change_preview_flag_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                        bool new_preview_flag) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(new_preview_flag, kTestPreviewFlag);
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.ChangePreviewFlag(kTestUniverse, kTestPreviewFlag).IsOk(), true);
  EXPECT_EQ(sacn_source_change_preview_flag_fake.call_count, 1u);
}

TEST_F(TestSource, ChangeSyncUniverseWorks)
{
  sacn_source_change_synchronization_universe_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                                    uint16_t new_sync_universe) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(new_sync_universe, kTestSyncUniverse);
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.ChangeSynchronizationUniverse(kTestUniverse, kTestSyncUniverse).IsOk(), true);
  EXPECT_EQ(sacn_source_change_synchronization_universe_fake.call_count, 1u);
}

TEST_F(TestSource, SendNowWorks)
{
  sacn_source_send_now_fake.custom_fake = [](sacn_source_t handle, uint16_t universe, uint8_t start_code,
                                             const uint8_t* buffer, size_t buflen) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(start_code, kTestStartCode);
    EXPECT_EQ(buffer, kTestBuffer.data());
    EXPECT_EQ(buflen, kTestBuffer.size());
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.SendNow(kTestUniverse, kTestStartCode, kTestBuffer.data(), kTestBuffer.size()).IsOk(), true);
  EXPECT_EQ(sacn_source_send_now_fake.call_count, 1u);
}

TEST_F(TestSource, SendSynchronizationWorks)
{
  sacn_source_send_synchronization_fake.custom_fake = [](sacn_source_t handle, uint16_t sync_universe) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(sync_universe, kTestSyncUniverse);
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.SendSynchronization(kTestSyncUniverse).IsOk(), true);
  EXPECT_EQ(sacn_source_send_synchronization_fake.call_count, 1u);
}

TEST_F(TestSource, UpdateValuesWorks)
{
  sacn_source_update_levels_fake.custom_fake = [](sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                                                  size_t new_values_size) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(new_values, kTestBuffer.data());
    EXPECT_EQ(new_values_size, kTestBuffer.size());
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  source.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  EXPECT_EQ(sacn_source_update_levels_fake.call_count, 1u);
}

TEST_F(TestSource, UpdateValuesAndPapWorks)
{
  sacn_source_update_levels_and_pap_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                          const uint8_t* new_values, size_t new_values_size,
                                                          const uint8_t* new_priorities, size_t new_priorities_size) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(new_values, kTestBuffer.data());
    EXPECT_EQ(new_values_size, kTestBuffer.size());
    EXPECT_EQ(new_priorities, kTestBuffer2.data());
    EXPECT_EQ(new_priorities_size, kTestBuffer2.size());
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  source.UpdateLevelsAndPap(kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer2.data(),
                            kTestBuffer2.size());
  EXPECT_EQ(sacn_source_update_levels_and_pap_fake.call_count, 1u);
}

TEST_F(TestSource, UpdateValuesAndForceSyncWorks)
{
  sacn_source_update_levels_and_force_sync_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                                 const uint8_t* new_values, size_t new_values_size) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(new_values, kTestBuffer.data());
    EXPECT_EQ(new_values_size, kTestBuffer.size());
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  source.UpdateLevelsAndForceSync(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  EXPECT_EQ(sacn_source_update_levels_and_force_sync_fake.call_count, 1u);
}

TEST_F(TestSource, UpdateValuesAndPapAndForceSyncWorks)
{
  sacn_source_update_levels_and_pap_and_force_sync_fake.custom_fake =
      [](sacn_source_t handle, uint16_t universe, const uint8_t* new_values, size_t new_values_size,
         const uint8_t* new_priorities, size_t new_priorities_size) {
        EXPECT_EQ(handle, kTestHandle);
        EXPECT_EQ(universe, kTestUniverse);
        EXPECT_EQ(new_values, kTestBuffer.data());
        EXPECT_EQ(new_values_size, kTestBuffer.size());
        EXPECT_EQ(new_priorities, kTestBuffer2.data());
        EXPECT_EQ(new_priorities_size, kTestBuffer2.size());
      };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  source.UpdateLevelsAndPapAndForceSync(kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), kTestBuffer2.data(),
                                        kTestBuffer2.size());
  EXPECT_EQ(sacn_source_update_levels_and_pap_and_force_sync_fake.call_count, 1u);
}

TEST_F(TestSource, ProcessManualWorks)
{
  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  source.ProcessManual();
  EXPECT_EQ(sacn_source_process_manual_fake.call_count, 1u);
}

TEST_F(TestSource, ResetNetworkingWorksWithoutNetints)
{
  sacn_source_reset_networking_fake.custom_fake = [](const SacnNetintConfig* netint_config) {
    if (netint_config)
    {
      EXPECT_EQ(netint_config->netints, nullptr);
      EXPECT_EQ(netint_config->num_netints, 0u);
      EXPECT_FALSE(netint_config->no_netints);
    }
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.ResetNetworking().IsOk(), true);
  EXPECT_EQ(sacn_source_reset_networking_fake.call_count, 1u);
  std::vector<SacnMcastInterface> empty_netints;
  EXPECT_EQ(source.ResetNetworking(empty_netints).IsOk(), true);
  EXPECT_EQ(sacn_source_reset_networking_fake.call_count, 2u);
}

TEST_F(TestSource, ResetNetworkingWorksWithNetints)
{
  sacn_source_reset_networking_fake.custom_fake = [](const SacnNetintConfig* netint_config) {
    EXPECT_NE(netint_config, nullptr);
    if (netint_config)
    {
      EXPECT_EQ(netint_config->netints, kTestNetints.data());
      EXPECT_EQ(netint_config->num_netints, kTestNetints.size());
    }
    return kEtcPalErrOk;
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.ResetNetworking(kTestNetints).IsOk(), true);
  EXPECT_EQ(sacn_source_reset_networking_fake.call_count, 1u);
}

TEST_F(TestSource, ResetNetworkingPerUniverseWorks)
{
  sacn_source_reset_networking_per_universe_fake.custom_fake =
      [](const SacnNetintConfig*, const SacnSourceUniverseNetintList* netint_lists, size_t num_netint_lists) {
        EXPECT_EQ(num_netint_lists, kTestNetintLists.size());

        for (size_t i = 0u; i < num_netint_lists; ++i)
        {
          EXPECT_EQ(netint_lists[i].handle, kTestNetintLists[i].handle);
          EXPECT_EQ(netint_lists[i].universe, kTestNetintLists[i].universe);
          EXPECT_EQ(netint_lists[i].netints, kTestNetintLists[i].netints.data());
          EXPECT_EQ(netint_lists[i].num_netints, kTestNetintLists[i].netints.size());
          EXPECT_EQ(netint_lists[i].no_netints, kTestNetintLists[i].no_netints);
        }

        return kEtcPalErrOk;
      };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  EXPECT_EQ(source.ResetNetworking(kTestNetints, kTestNetintLists).IsOk(), true);
  EXPECT_EQ(sacn_source_reset_networking_per_universe_fake.call_count, 1u);
}

TEST_F(TestSource, GetGrowingNetintsWorks)
{
  sacn_source_get_network_interfaces_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                           EtcPalMcastNetintId* netints, size_t netints_size) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_NE(netints, nullptr);
    EXPECT_EQ(netints_size, current_netints.size() + 4u);

    for (int i = 0; (i < 5) && (current_netints.size() < kTestNetints.size()); ++i)
      current_netints.push_back(kTestNetints[current_netints.size()]);

    if (sacn_source_get_network_interfaces_fake.call_count < 4u)
      EXPECT_LT(netints_size, current_netints.size());
    else
      EXPECT_GT(netints_size, current_netints.size());

    for (size_t i = 0; (i < netints_size) && (i < current_netints.size()); ++i)
      netints[i] = current_netints[i].iface;

    return current_netints.size();
  };

  current_netints.clear();

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  std::vector<EtcPalMcastNetintId> result = source.GetNetworkInterfaces(kTestUniverse);
  for (size_t i = 0u; i < result.size(); ++i)
  {
    EXPECT_EQ(result[i].index, kTestNetints[i].iface.index);
    EXPECT_EQ(result[i].ip_type, kTestNetints[i].iface.ip_type);
  }

  EXPECT_EQ(sacn_source_get_network_interfaces_fake.call_count, 4u);
}

TEST_F(TestSource, GetUnchangingNetintsWorks)
{
  sacn_source_get_network_interfaces_fake.custom_fake = [](sacn_source_t handle, uint16_t universe,
                                                           EtcPalMcastNetintId* netints, size_t netints_size) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_NE(netints, nullptr);

    if (sacn_source_get_network_interfaces_fake.call_count == 1u)
      EXPECT_EQ(netints_size, 4u);
    else
      EXPECT_EQ(netints_size, kTestNetints.size() + 4u);

    for (size_t i = 0; (i < netints_size) && (i < kTestNetints.size()); ++i)
      netints[i] = kTestNetints[i].iface;

    return kTestNetints.size();
  };

  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));

  std::vector<EtcPalMcastNetintId> result = source.GetNetworkInterfaces(kTestUniverse);
  for (size_t i = 0u; i < result.size(); ++i)
  {
    EXPECT_EQ(result[i].index, kTestNetints[i].iface.index);
    EXPECT_EQ(result[i].ip_type, kTestNetints[i].iface.ip_type);
  }

  EXPECT_EQ(sacn_source_get_network_interfaces_fake.call_count, 2u);
}

TEST_F(TestSource, GetHandleWorks)
{
  sacn::Source source;
  source.Startup(sacn::Source::Settings(kTestLocalCid, kTestLocalName));
  EXPECT_EQ(source.handle().value(), kTestHandle);
}
