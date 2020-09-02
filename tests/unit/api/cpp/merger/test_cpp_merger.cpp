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

#include "sacn/cpp/common.h"
#include "sacn/cpp/dmx_merger.h"

#include <limits>
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/data_loss.h"
#include "sacn_mock/private/sockets.h"
#include "sacn_mock/private/dmx_merger.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
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
    sacn_reset_all_fakes();

    sacn_dmx_merger_create_fake.custom_fake = [](const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle) {
      if (handle)
      {
        *handle = kTestMergerHandle;
      }

      return kEtcPalErrOk;
    };

    test_return_value_ = kEtcPalErrSys;
    test_source_handle_ = 456;
    test_source_cid_ = etcpal::Uuid::FromString("0123456789abcdef");
    test_priority_ = 123;

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_dmx_merger_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_dmx_merger_deinit();
    sacn_mem_deinit();
  }

  static const sacn::DmxMerger::Handle kTestMergerHandle = 123;
  static const sacn::DmxMerger::Handle kTestNewValuesCount = DMX_ADDRESS_COUNT;
  static const sacn::DmxMerger::Handle kTestAddressPrioritiesCount = DMX_ADDRESS_COUNT;

  static etcpal_error_t test_return_value_;
  static sacn_source_id_t test_source_handle_;
  static etcpal::Uuid test_source_cid_;
  static SacnDmxMergerSource test_source_;
  static uint8_t test_priority_;
  static uint8_t test_new_values_[DMX_ADDRESS_COUNT];
  static uint8_t test_address_priorities_[DMX_ADDRESS_COUNT];
  static SacnHeaderData test_header_;
  static uint8_t test_pdata_[DMX_ADDRESS_COUNT];

  static uint8_t slots_[DMX_ADDRESS_COUNT];
  static sacn_source_id_t slot_owners_[DMX_ADDRESS_COUNT];
  static sacn::DmxMerger::Settings settings_default_;
};

etcpal_error_t TestMerger::test_return_value_;
sacn_source_id_t TestMerger::test_source_handle_;
etcpal::Uuid TestMerger::test_source_cid_;
SacnDmxMergerSource TestMerger::test_source_;
uint8_t TestMerger::test_priority_;
uint8_t TestMerger::test_new_values_[];
uint8_t TestMerger::test_address_priorities_[];
SacnHeaderData TestMerger::test_header_;
uint8_t TestMerger::test_pdata_[];

uint8_t TestMerger::slots_[];
sacn_source_id_t TestMerger::slot_owners_[];
sacn::DmxMerger::Settings TestMerger::settings_default_(nullptr, nullptr);

TEST_F(TestMerger, SettingsConstructorWorks)
{
  sacn::DmxMerger::Settings settings(slots_, slot_owners_);

  EXPECT_EQ(settings.slots, slots_);
  EXPECT_EQ(settings.slot_owners, slot_owners_);
}

TEST_F(TestMerger, SettingsIsValidWorks)
{
  sacn::DmxMerger::Settings settings_valid(slots_, slot_owners_);
  sacn::DmxMerger::Settings settings_invalid_1(nullptr, slot_owners_);
  sacn::DmxMerger::Settings settings_invalid_2(slots_, nullptr);
  sacn::DmxMerger::Settings settings_invalid_3(nullptr, nullptr);

  EXPECT_EQ(settings_valid.IsValid(), true);
  EXPECT_EQ(settings_invalid_1.IsValid(), false);
  EXPECT_EQ(settings_invalid_2.IsValid(), false);
  EXPECT_EQ(settings_invalid_3.IsValid(), false);
}

TEST_F(TestMerger, StartupWorks)
{
  sacn_dmx_merger_create_fake.custom_fake = [](const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle) {
    EXPECT_NE(config, nullptr);
    EXPECT_NE(handle, nullptr);

    if (config)
    {
      EXPECT_EQ(config->slots, slots_);
      EXPECT_EQ(config->slot_owners, slot_owners_);
      EXPECT_EQ(config->source_count_max, SACN_RECEIVER_INFINITE_SOURCES);
    }

    if (handle)
    {
      *handle = kTestMergerHandle;
    }

    return test_return_value_;
  };

  sacn::DmxMerger merger;

  etcpal::Error result = merger.Startup(sacn::DmxMerger::Settings(slots_, slot_owners_));

  EXPECT_EQ(sacn_dmx_merger_create_fake.call_count, 1);
  EXPECT_EQ(merger.handle(), kTestMergerHandle);
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

  EXPECT_EQ(sacn_dmx_merger_destroy_fake.call_count, 1);
  EXPECT_EQ(merger.handle(), sacn::DmxMerger::kInvalidHandle);
}

TEST_F(TestMerger, AddSourceWorks)
{
  sacn_dmx_merger_add_source_fake.custom_fake = [](sacn_dmx_merger_t merger, const EtcPalUuid* source_cid,
                                                   sacn_source_id_t* source_id) {
    EXPECT_NE(source_cid, nullptr);
    EXPECT_NE(source_id, nullptr);

    EXPECT_EQ(merger, kTestMergerHandle);

    if (source_cid)
    {
      EXPECT_EQ(memcmp(source_cid->data, test_source_cid_.data(), ETCPAL_UUID_BYTES), 0);
    }

    if (source_id)
    {
      *source_id = test_source_handle_;
    }

    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  test_return_value_ = kEtcPalErrOk;
  auto result_ok = merger.AddSource(test_source_cid_);

  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 1);
  ASSERT_EQ(result_ok.has_value(), true);
  EXPECT_EQ(result_ok.value(), test_source_handle_);

  test_return_value_ = kEtcPalErrSys;
  auto result_error = merger.AddSource(test_source_cid_);

  EXPECT_EQ(sacn_dmx_merger_add_source_fake.call_count, 2);
  ASSERT_EQ(result_error.has_value(), false);
  EXPECT_EQ(result_error.error_code(), test_return_value_);
}

TEST_F(TestMerger, RemoveSourceWorks)
{
  sacn_dmx_merger_remove_source_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_source_id_t source) {
    EXPECT_EQ(merger, kTestMergerHandle);
    EXPECT_EQ(source, test_source_handle_);
    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);
  etcpal::Error result = merger.RemoveSource(test_source_handle_);

  EXPECT_EQ(sacn_dmx_merger_remove_source_fake.call_count, 1);
  EXPECT_EQ(result.code(), test_return_value_);
}

TEST_F(TestMerger, GetSourceIdWorks)
{
  sacn_dmx_merger_get_id_fake.custom_fake = [](sacn_dmx_merger_t merger, const EtcPalUuid* source_cid) {
    EXPECT_NE(source_cid, nullptr);

    EXPECT_EQ(merger, kTestMergerHandle);

    if (source_cid)
    {
      EXPECT_EQ(memcmp(source_cid->data, test_source_cid_.data(), ETCPAL_UUID_BYTES), 0);
    }

    return test_source_handle_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  auto result_ok = merger.GetSourceId(test_source_cid_);
  EXPECT_EQ(sacn_dmx_merger_get_id_fake.call_count, 1);
  ASSERT_EQ(result_ok.has_value(), true);
  EXPECT_EQ(result_ok.value(), test_source_handle_);

  test_source_handle_ = SACN_DMX_MERGER_SOURCE_INVALID;
  auto result_error = merger.GetSourceId(test_source_cid_);
  EXPECT_EQ(sacn_dmx_merger_get_id_fake.call_count, 2);
  ASSERT_EQ(result_error.has_value(), false);
  EXPECT_EQ(result_error.error_code(), kEtcPalErrInvalid);
}

TEST_F(TestMerger, GetSourceInfoWorks)
{
  sacn_dmx_merger_get_source_fake.custom_fake = [](sacn_dmx_merger_t merger, sacn_source_id_t source) {
    EXPECT_EQ(merger, kTestMergerHandle);
    EXPECT_EQ(source, test_source_handle_);
    return const_cast<const SacnDmxMergerSource*>(&test_source_);
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);
  auto result = merger.GetSourceInfo(test_source_handle_);
  EXPECT_EQ(sacn_dmx_merger_get_source_fake.call_count, 1);
  EXPECT_EQ(result, &test_source_);
}

TEST_F(TestMerger, UpdateSourceDataWorks)
{
  sacn_dmx_merger_update_source_data_fake.custom_fake =
      [](sacn_dmx_merger_t merger, sacn_source_id_t source, uint8_t priority, const uint8_t* new_values,
         size_t new_values_count, const uint8_t* address_priorities, size_t address_priorities_count) {
        EXPECT_EQ(merger, kTestMergerHandle);
        EXPECT_EQ(source, test_source_handle_);
        EXPECT_EQ(priority, test_priority_);
        EXPECT_EQ(new_values, test_new_values_);
        EXPECT_EQ(new_values_count, kTestNewValuesCount);
        EXPECT_EQ(address_priorities, test_address_priorities_);
        EXPECT_EQ(address_priorities_count, kTestAddressPrioritiesCount);

        return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  etcpal::Error result =
      merger.UpdateSourceData(test_source_handle_, test_priority_, test_new_values_, kTestNewValuesCount,
                              test_address_priorities_, kTestAddressPrioritiesCount);

  EXPECT_EQ(sacn_dmx_merger_update_source_data_fake.call_count, 1);
  EXPECT_EQ(result.code(), test_return_value_);
}

TEST_F(TestMerger, UpdateSourceDataFromSacnWorks)
{
  sacn_dmx_merger_update_source_from_sacn_fake.custom_fake = [](sacn_dmx_merger_t merger, const SacnHeaderData* header,
                                                                const uint8_t* pdata) {
    EXPECT_EQ(merger, kTestMergerHandle);
    EXPECT_EQ(header, &test_header_);
    EXPECT_EQ(pdata, test_pdata_);
    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  etcpal::Error result = merger.UpdateSourceDataFromSacn(test_header_, test_pdata_);

  EXPECT_EQ(sacn_dmx_merger_update_source_from_sacn_fake.call_count, 1);
  EXPECT_EQ(result.code(), test_return_value_);
}

TEST_F(TestMerger, StopSourcePapWorks)
{
  sacn_dmx_merger_stop_source_per_address_priority_fake.custom_fake = [](sacn_dmx_merger_t merger,
                                                                         sacn_source_id_t source) {
    EXPECT_EQ(merger, kTestMergerHandle);
    EXPECT_EQ(source, test_source_handle_);
    return test_return_value_;
  };

  sacn::DmxMerger merger;

  merger.Startup(settings_default_);

  etcpal::Error result = merger.StopSourcePerAddressPriority(test_source_handle_);

  EXPECT_EQ(sacn_dmx_merger_stop_source_per_address_priority_fake.call_count, 1);
  EXPECT_EQ(result.code(), test_return_value_);
}
