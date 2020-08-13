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

#include "sacn/dmx_merger.h"

#include <limits>
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/data_loss.h"
#include "sacn_mock/private/sockets.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/dmx_merger.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestDmxMerger TestDmxMergerDynamic
#else
#define TestDmxMerger TestDmxMergerStatic
#endif

constexpr uint16_t VALID_UNIVERSE_ID = 1;
constexpr uint16_t INVALID_UNIVERSE_ID = 0;
constexpr uint8_t VALID_PRIORITY = 100;
constexpr uint8_t INVALID_PRIORITY = 201;

class TestDmxMerger : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_dmx_merger_init(), kEtcPalErrOk);

    header_default_.cid = etcpal::Uuid::V4().get();
    memset(header_default_.source_name, '\0', SACN_SOURCE_NAME_MAX_LEN);
    header_default_.universe_id = VALID_UNIVERSE_ID;
    header_default_.priority = VALID_PRIORITY;
    header_default_.preview = false;
    header_default_.start_code = 0x00;
    header_default_.slot_count = DMX_ADDRESS_COUNT;
    memset(pdata_default_, 0, DMX_ADDRESS_COUNT);

    merger_config_ = SACN_DMX_MERGER_CONFIG_INIT;
    merger_config_.slots = slots_;
    merger_config_.slot_owners = slot_owners_;
    merger_config_.source_count_max = SACN_RECEIVER_INFINITE_SOURCES;

    const char* ns_str = "1234567890abcdef";
    memcpy(namespace_uuid_.data, ns_str, ETCPAL_UUID_BYTES);
  }

  void TearDown() override
  {
    sacn_dmx_merger_deinit();
    sacn_mem_deinit();
  }

  void GenV5(int iteration, EtcPalUuid* uuid)
  {
    char name[80];
    sprintf(name, "%d", iteration);

    etcpal_generate_v5_uuid(&namespace_uuid_, name, 80, uuid);
  }

  SacnHeaderData header_default_;
  uint8_t pdata_default_[DMX_ADDRESS_COUNT];

  uint8_t slots_[DMX_ADDRESS_COUNT];
  source_id_t slot_owners_[DMX_ADDRESS_COUNT];
  sacn_dmx_merger_t merger_handle_;
  SacnDmxMergerConfig merger_config_;

  EtcPalUuid namespace_uuid_;
};

TEST_F(TestDmxMerger, MergerCreateErrInvalidWorks)
{
  uint8_t slots[DMX_ADDRESS_COUNT];
  source_id_t slot_owners[DMX_ADDRESS_COUNT];
  sacn_dmx_merger_t handle;

  SacnDmxMergerConfig invalidSlotsConfig;
  invalidSlotsConfig.slots = NULL;
  invalidSlotsConfig.slot_owners = slot_owners;

  SacnDmxMergerConfig invalidSlotOwnersConfig;
  invalidSlotOwnersConfig.slots = slots;
  invalidSlotOwnersConfig.slot_owners = NULL;

  SacnDmxMergerConfig validConfig;
  validConfig.slots = slots;
  validConfig.slot_owners = slot_owners;

  etcpal_error_t null_config_result = sacn_dmx_merger_create(NULL, &handle);
  etcpal_error_t null_handle_result = sacn_dmx_merger_create(&validConfig, NULL);
  etcpal_error_t null_slots_result = sacn_dmx_merger_create(&invalidSlotsConfig, &handle);
  etcpal_error_t null_slot_owners_result = sacn_dmx_merger_create(&invalidSlotOwnersConfig, &handle);

  etcpal_error_t valid_result = sacn_dmx_merger_create(&validConfig, &handle);

  EXPECT_EQ(null_config_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_handle_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_slots_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_slot_owners_result, kEtcPalErrInvalid);

  EXPECT_NE(valid_result, kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, MergerCreateErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_create(NULL, NULL);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_create(NULL, NULL);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, AddSourceErrInvalidWorks)
{
  // Initialize a merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Run tests.
  EtcPalUuid source_cid;
  source_id_t source_handle;

  etcpal_error_t null_cid_result = sacn_dmx_merger_add_source(merger_handle_, NULL, &source_handle);
  etcpal_error_t null_source_handle_result = sacn_dmx_merger_add_source(merger_handle_, &source_cid, NULL);
  etcpal_error_t unknown_merger_handle_result =
      sacn_dmx_merger_add_source(merger_handle_ + 1, &source_cid, &source_handle);

  etcpal_error_t valid_result = sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle);

  EXPECT_EQ(null_cid_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_source_handle_result, kEtcPalErrInvalid);
  EXPECT_EQ(unknown_merger_handle_result, kEtcPalErrInvalid);

  EXPECT_NE(valid_result, kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, AddSourceErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_add_source(0, NULL, NULL);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_add_source(0, NULL, NULL);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, AddSourceErrNoMemWorks)
{
  // Initialize a merger.
  merger_config_.source_count_max = SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER;
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Add up to the maximum number of sources.
  EtcPalUuid source_cid;
  source_id_t source_handle;

  for (int i = 0; i < SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER; ++i)
  {
    GenV5(i, &source_cid);
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle), kEtcPalErrOk);
  }

  // Now add one more source and make sure it fails.
  GenV5(SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER, &source_cid);
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle), kEtcPalErrNoMem);

  // Set source_count_max to infinite, which should allow it to work, but only with dynamic memory.
  merger_config_.source_count_max = SACN_RECEIVER_INFINITE_SOURCES;

#if SACN_DYNAMIC_MEM
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle), kEtcPalErrOk);
#else
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle), kEtcPalErrNoMem);
#endif
}

TEST_F(TestDmxMerger, AddSourceErrExistsWorks)
{
  // Initialize a merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Initialize a source.
  const char* cid_str_1 = "1234567890abcdef";

  EtcPalUuid source_cid_1;
  source_id_t source_handle_1;

  memcpy(source_cid_1.data, cid_str_1, ETCPAL_UUID_BYTES);

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_1, &source_handle_1), kEtcPalErrOk);

  // Try to add another source with the same CID.
  EtcPalUuid source_cid_2;
  source_id_t source_handle_2;

  memcpy(source_cid_2.data, cid_str_1, ETCPAL_UUID_BYTES);

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_2, &source_handle_2), kEtcPalErrExists);

  // Try to add another source with a different CID.
  const char* cid_str_2 = "abcdef1234567890";

  EtcPalUuid source_cid_3;
  source_id_t source_handle_3;

  memcpy(source_cid_3.data, cid_str_2, ETCPAL_UUID_BYTES);

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_3, &source_handle_3), kEtcPalErrOk);
}

TEST_F(TestDmxMerger, UpdateSourceDataErrInvalidWorks)
{
  uint8_t foo = 0;

  etcpal_error_t invalid_source_result =
      sacn_dmx_merger_update_source_data(0, SACN_DMX_MERGER_SOURCE_INVALID, nullptr, 0, VALID_PRIORITY, nullptr, 0);
  etcpal_error_t invalid_new_values_result =
      sacn_dmx_merger_update_source_data(0, 0, &foo, 0, VALID_PRIORITY, nullptr, 0);
  etcpal_error_t invalid_new_values_count_result_1 =
      sacn_dmx_merger_update_source_data(0, 0, nullptr, 1, VALID_PRIORITY, nullptr, 0);
  etcpal_error_t invalid_new_values_count_result_2 =
      sacn_dmx_merger_update_source_data(0, 0, &foo, DMX_ADDRESS_COUNT + 1, VALID_PRIORITY, nullptr, 0);
  etcpal_error_t invalid_priority_result =
      sacn_dmx_merger_update_source_data(0, 0, nullptr, 0, INVALID_PRIORITY, nullptr, 0);
  etcpal_error_t invalid_address_priorities_result =
      sacn_dmx_merger_update_source_data(0, 0, nullptr, 0, VALID_PRIORITY, &foo, 0);
  etcpal_error_t invalid_address_priorities_count_result_1 =
      sacn_dmx_merger_update_source_data(0, 0, nullptr, 0, VALID_PRIORITY, nullptr, 1);
  etcpal_error_t invalid_address_priorities_count_result_2 =
      sacn_dmx_merger_update_source_data(0, 0, nullptr, 0, VALID_PRIORITY, &foo, DMX_ADDRESS_COUNT + 1);

  etcpal_error_t valid_result_1 = sacn_dmx_merger_update_source_data(0, 0, nullptr, 0, VALID_PRIORITY, nullptr, 0);
  etcpal_error_t valid_result_2 = sacn_dmx_merger_update_source_data(0, 0, &foo, 1, VALID_PRIORITY, nullptr, 0);
  etcpal_error_t valid_result_3 = sacn_dmx_merger_update_source_data(0, 0, nullptr, 0, VALID_PRIORITY, &foo, 1);
  etcpal_error_t valid_result_4 = sacn_dmx_merger_update_source_data(0, 0, &foo, 1, VALID_PRIORITY, &foo, 1);

  EXPECT_EQ(invalid_source_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_new_values_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_new_values_count_result_1, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_new_values_count_result_2, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_priority_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_address_priorities_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_address_priorities_count_result_1, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_address_priorities_count_result_2, kEtcPalErrInvalid);

  EXPECT_NE(valid_result_1, kEtcPalErrInvalid);
  EXPECT_NE(valid_result_2, kEtcPalErrInvalid);
  EXPECT_NE(valid_result_3, kEtcPalErrInvalid);
  EXPECT_NE(valid_result_4, kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, UpdateSourceDataErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_update_source_data(0, 0, nullptr, 0, 0, nullptr, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_update_source_data(0, 0, nullptr, 0, 0, nullptr, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, UpdateSourceDataErrNotFoundWorks)
{
  source_id_t source = 0;

  etcpal_error_t no_merger_result =
      sacn_dmx_merger_update_source_data(merger_handle_, source, nullptr, 0, VALID_PRIORITY, nullptr, 0);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result =
      sacn_dmx_merger_update_source_data(merger_handle_, source, nullptr, 0, VALID_PRIORITY, nullptr, 0);

  sacn_dmx_merger_add_source(merger_handle_, &header_default_.cid, &source);

  etcpal_error_t found_result =
      sacn_dmx_merger_update_source_data(merger_handle_, source, nullptr, 0, VALID_PRIORITY, nullptr, 0);

  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnErrInvalidWorks)
{
  SacnHeaderData invalid_cid_header = header_default_;
  SacnHeaderData invalid_universe_header = header_default_;
  SacnHeaderData invalid_priority_header = header_default_;
  SacnHeaderData invalid_slot_count_header = header_default_;

  invalid_cid_header.cid = kEtcPalNullUuid;
  invalid_universe_header.universe_id = INVALID_UNIVERSE_ID;
  invalid_priority_header.priority = INVALID_PRIORITY;
  invalid_slot_count_header.slot_count = DMX_ADDRESS_COUNT + 1;

  etcpal_error_t null_header_result = sacn_dmx_merger_update_source_from_sacn(0, nullptr, pdata_default_);
  etcpal_error_t invalid_cid_result = sacn_dmx_merger_update_source_from_sacn(0, &invalid_cid_header, pdata_default_);
  etcpal_error_t invalid_universe_result =
      sacn_dmx_merger_update_source_from_sacn(0, &invalid_universe_header, pdata_default_);
  etcpal_error_t invalid_priority_result =
      sacn_dmx_merger_update_source_from_sacn(0, &invalid_priority_header, pdata_default_);
  etcpal_error_t invalid_slot_count_result =
      sacn_dmx_merger_update_source_from_sacn(0, &invalid_slot_count_header, pdata_default_);
  etcpal_error_t null_pdata_result = sacn_dmx_merger_update_source_from_sacn(0, &header_default_, nullptr);
  etcpal_error_t valid_result = sacn_dmx_merger_update_source_from_sacn(0, &header_default_, pdata_default_);

  EXPECT_EQ(null_header_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_cid_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_universe_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_priority_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_slot_count_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_pdata_result, kEtcPalErrInvalid);

  EXPECT_NE(valid_result, kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_update_source_from_sacn(0, &header_default_, pdata_default_);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_update_source_from_sacn(0, &header_default_, pdata_default_);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnErrNotFoundWorks)
{
  source_id_t source;
  SacnHeaderData header = header_default_;

  etcpal_error_t no_merger_result = sacn_dmx_merger_update_source_from_sacn(merger_handle_, &header, pdata_default_);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result = sacn_dmx_merger_update_source_from_sacn(merger_handle_, &header, pdata_default_);

  sacn_dmx_merger_add_source(merger_handle_, &header.cid, &source);

  etcpal_error_t found_result = sacn_dmx_merger_update_source_from_sacn(merger_handle_, &header, pdata_default_);

  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}
