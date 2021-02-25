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

#include "sacn/cpp/common.h"
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
static constexpr sacn_source_t kTestHandle = 456;

static std::vector<SacnMcastInterface> kTestNetints = {{{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}};
static std::vector<SacnMcastInterface> kTestNetintsEmpty = {};

static const std::vector<uint16_t> kTestUniverses = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u};

static std::vector<uint16_t> current_universes;

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

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
  }

  void TearDown() override { sacn_mem_deinit(); }
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
                                                 SacnMcastInterface* netints, size_t num_netints) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(config->universe, kTestUniverse);
    EXPECT_EQ(config->priority, 100u);
    EXPECT_EQ(config->send_preview, false);
    EXPECT_EQ(config->send_unicast_only, false);
    EXPECT_EQ(config->unicast_destinations, nullptr);
    EXPECT_EQ(config->num_unicast_destinations, 0u);
    EXPECT_EQ(config->sync_universe, 0u);
    EXPECT_EQ(netints, nullptr);
    EXPECT_EQ(num_netints, 0u);
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
                                                 SacnMcastInterface* netints, size_t num_netints) {
    EXPECT_EQ(handle, kTestHandle);
    EXPECT_EQ(config->universe, kTestUniverse);
    EXPECT_EQ(config->priority, 100u);
    EXPECT_EQ(config->send_preview, false);
    EXPECT_EQ(config->send_unicast_only, false);
    EXPECT_EQ(config->unicast_destinations, nullptr);
    EXPECT_EQ(config->num_unicast_destinations, 0u);
    EXPECT_EQ(config->sync_universe, 0u);
    EXPECT_EQ(netints, kTestNetints.data());
    EXPECT_EQ(num_netints, kTestNetints.size());
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

TEST_F(TestSource, GetStaticUniversesWorks)
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
