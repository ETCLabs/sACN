/******************************************************************************
 * Copyright 2024 ETC Inc.
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

#include "sacn/cpp/dmx_merger.h"

#include <limits>
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/source_loss.h"
#include "sacn_mock/private/sockets.h"
#include "sacn_mock/private/dmx_merger.h"
#include "sacn/private/mem.h"
#include "sacn/opts.h"
#include "gtest/gtest.h"

#if SACN_DYNAMIC_MEM
#define TestMerger TestCppMergerDynamic
#else
#define TestMerger TestCppMergerStatic
#endif

class TestMerger : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_source_loss_reset_all_fakes();
    sacn_sockets_reset_all_fakes();
    sacn_dmx_merger_reset_all_fakes();

    sacn_dmx_merger_create_fake.custom_fake = [](const SacnDmxMergerConfig* /*config*/, sacn_dmx_merger_t* handle) {
      if (handle)
        *handle = kTestMergerHandle;

      return kEtcPalErrOk;
    };

    test_return_value_  = kEtcPalErrSys;
    test_source_handle_ = 456;

    ASSERT_EQ(sacn_dmx_merger_init(), kEtcPalErrOk);
  }

  void TearDown() override { sacn_dmx_merger_deinit(); }

  static constexpr sacn_dmx_merger_t kTestMergerHandle           = 123;
  static constexpr uint8_t           kTestPriority               = 123;
  static constexpr size_t            kTestNewValuesCount         = 123;
  static constexpr size_t            kTestAddressPrioritiesCount = 456;

  static const SacnDmxMergerSource kTestSource;
  static const uint8_t             kTestNewValues[SACN_DMX_MERGER_MAX_SLOTS];
  static const uint8_t             kTestAddressPriorities[SACN_DMX_MERGER_MAX_SLOTS];
  static const uint8_t             kTestPdata[SACN_DMX_MERGER_MAX_SLOTS];

  static uint8_t                  levels_[SACN_DMX_MERGER_MAX_SLOTS];
  static uint8_t                  pap_[SACN_DMX_MERGER_MAX_SLOTS];
  static sacn_dmx_merger_source_t owners_[SACN_DMX_MERGER_MAX_SLOTS];

  static etcpal_error_t            test_return_value_;
  static sacn_dmx_merger_source_t  test_source_handle_;
  static sacn::DmxMerger::Settings settings_default_;
};

const SacnDmxMergerSource TestMerger::kTestSource              = {0};
const uint8_t             TestMerger::kTestNewValues[]         = {};
const uint8_t             TestMerger::kTestAddressPriorities[] = {};
const uint8_t             TestMerger::kTestPdata[]             = {};

etcpal_error_t            TestMerger::test_return_value_;
sacn_dmx_merger_source_t  TestMerger::test_source_handle_;
uint8_t                   TestMerger::levels_[] = {};
uint8_t                   TestMerger::pap_[]    = {};
sacn_dmx_merger_source_t  TestMerger::owners_[] = {};
sacn::DmxMerger::Settings TestMerger::settings_default_(nullptr);

TEST_F(TestMerger, SettingsConstructorWorks)
{
  sacn::DmxMerger::Settings settings(levels_);
  EXPECT_EQ(settings.levels, levels_);
  EXPECT_EQ(settings.per_address_priorities, nullptr);
  EXPECT_EQ(settings.owners, nullptr);
}

TEST_F(TestMerger, SettingsIsValidWorks)
{
  sacn::DmxMerger::Settings settings_valid(levels_);
  sacn::DmxMerger::Settings settings_invalid(nullptr);

  EXPECT_EQ(settings_valid.IsValid(), true);
  EXPECT_EQ(settings_invalid.IsValid(), false);
}

TEST_F(TestMerger, StartupWorks)
{
  sacn_dmx_merger_create_fake.custom_fake = [](const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle) {
    EXPECT_NE(config, nullptr);
    EXPECT_NE(handle, nullptr);

    if (config)
    {
      EXPECT_EQ(config->levels, levels_);
      EXPECT_EQ(config->per_address_priorities, pap_);
      EXPECT_EQ(config->owners, owners_);
      EXPECT_EQ(config->source_count_max, SACN_RECEIVER_INFINITE_SOURCES);
    }

    if (handle)
      *handle = kTestMergerHandle;

    return test_return_value_;
  };

  sacn::DmxMerger merger;

  sacn::DmxMerger::Settings settings(levels_);
  settings.per_address_priorities = pap_;
  settings.owners                 = owners_;

  etcpal::Error result = merger.Startup(settings);

  EXPECT_EQ(sacn_dmx_merger_create_fake.call_count, 1u);
  EXPECT_EQ(merger.handle().value(), kTestMergerHandle);
  EXPECT_EQ(result.code(), test_return_value_);
}

TEST_F(TestMerger, ShutdownWorks)
{
  sacn_dmx_merger_destroy_fake.custom_fake = [](sacn_dmx_merger_t handle) {
    EXPECT_EQ(handle, kTestMergerHandle);
    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);
  merger.Shutdown();

  EXPECT_EQ(sacn_dmx_merger_destroy_fake.call_count, 1u);
  EXPECT_FALSE(merger.handle().IsValid());
}

TEST_F(TestMerger, AddSourceWorks)
{
  sacn_dmx_merger_add_source_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t* source_id) {
    EXPECT_NE(source_id, nullptr);

    EXPECT_EQ(merger, kTestMergerHandle);

    if (source_id)
      *source_id = test_source_handle_;

    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  test_return_value_ = kEtcPalErrOk;
  auto result_ok     = merger.AddSource();

  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 1u);
  ASSERT_EQ(result_ok.has_value(), true);
  EXPECT_EQ(result_ok.value(), test_source_handle_);

  test_return_value_ = kEtcPalErrSys;
  auto result_error  = merger.AddSource();

  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 2u);
  ASSERT_EQ(result_error.has_value(), false);
  EXPECT_EQ(result_error.error_code(), test_return_value_);
}

TEST_F(TestMerger, RemoveSourceWorks)
{
  sacn_dmx_merger_remove_source_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source) {
    EXPECT_EQ(merger, kTestMergerHandle);
    EXPECT_EQ(source, test_source_handle_);
    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);
  etcpal::Error result = merger.RemoveSource(test_source_handle_);

  EXPECT_EQ(sacn_dmx_merger_remove_source_fake.call_count, 1u);
  EXPECT_EQ(result.code(), test_return_value_);
}

TEST_F(TestMerger, GetSourceInfoWorks)
{
  sacn_dmx_merger_get_source_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source) {
    EXPECT_EQ(merger, kTestMergerHandle);
    EXPECT_EQ(source, test_source_handle_);
    return &kTestSource;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);
  auto result = merger.GetSourceInfo(test_source_handle_);
  EXPECT_EQ(sacn_dmx_merger_get_source_fake.call_count, 1u);
  EXPECT_EQ(result, &kTestSource);
}

TEST_F(TestMerger, UpdateLevelsWorks)
{
  sacn_dmx_merger_update_levels_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                                      const uint8_t* new_levels, size_t new_levels_count) {
    EXPECT_EQ(merger, kTestMergerHandle);
    EXPECT_EQ(source, test_source_handle_);
    EXPECT_EQ(new_levels, kTestNewValues);
    EXPECT_EQ(new_levels_count, kTestNewValuesCount);

    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  etcpal::Error result = merger.UpdateLevels(test_source_handle_, kTestNewValues, kTestNewValuesCount);

  EXPECT_EQ(sacn_dmx_merger_update_levels_fake.call_count, 1u);
  EXPECT_EQ(result.code(), test_return_value_);
}

TEST_F(TestMerger, UpdatePapWorks)
{
  sacn_dmx_merger_update_pap_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                                   const uint8_t* pap, size_t pap_count) {
    EXPECT_EQ(merger, kTestMergerHandle);
    EXPECT_EQ(source, test_source_handle_);
    EXPECT_EQ(pap, kTestAddressPriorities);
    EXPECT_EQ(pap_count, kTestAddressPrioritiesCount);

    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  etcpal::Error result = merger.UpdatePap(test_source_handle_, kTestAddressPriorities, kTestAddressPrioritiesCount);

  EXPECT_EQ(sacn_dmx_merger_update_pap_fake.call_count, 1u);
  EXPECT_EQ(result.code(), test_return_value_);
}

TEST_F(TestMerger, UpdateUniversePriorityWorks)
{
  sacn_dmx_merger_update_universe_priority_fake.custom_fake =
      [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, uint8_t universe_priority) {
        EXPECT_EQ(merger, kTestMergerHandle);
        EXPECT_EQ(source, test_source_handle_);
        EXPECT_EQ(universe_priority, kTestPriority);

        return test_return_value_;
      };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  etcpal::Error result = merger.UpdateUniversePriority(test_source_handle_, kTestPriority);

  EXPECT_EQ(sacn_dmx_merger_update_universe_priority_fake.call_count, 1u);
  EXPECT_EQ(result.code(), test_return_value_);
}

TEST_F(TestMerger, StopSourcePapWorks)
{
  sacn_dmx_merger_remove_pap_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source) {
    EXPECT_EQ(merger, kTestMergerHandle);
    EXPECT_EQ(source, test_source_handle_);
    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  etcpal::Error result = merger.RemovePap(test_source_handle_);

  EXPECT_EQ(sacn_dmx_merger_remove_pap_fake.call_count, 1u);
  EXPECT_EQ(result.code(), test_return_value_);
}
