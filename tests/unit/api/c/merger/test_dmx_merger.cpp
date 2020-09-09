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

    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      test_values_ascending_[i] = i % 256;
      test_values_descending_[i] = 255 - test_values_ascending_[i];
    }
  }

  void TearDown() override
  {
    sacn_dmx_merger_deinit();
    sacn_mem_deinit();
  }

  void GenV5(int iteration, EtcPalUuid* uuid)
  {
    char name[80];
    sprintf_s(name, "%d", iteration);

    etcpal_generate_v5_uuid(&namespace_uuid_, name, 80, uuid);
  }

  EtcPalUuid GenV5(int iteration)
  {
    EtcPalUuid result;
    GenV5(iteration, &result);
    return result;
  }

  // This determines what kind of merge test TestMerge does.
  enum class MergeTestType
  {
    kUpdateSourceData,      // Merge using sacn_dmx_merger_update_source_data.
    kUpdateSourceFromSacn,  // Merge using sacn_dmx_merger_update_source_from_sacn.
    kStopSourcePap          // Merge using sacn_dmx_merger_update_source_data, then call
                            // sacn_dmx_merger_stop_source_per_address_priority on the second source.
  };

  void TestMerge(uint8_t priority_1, const uint8_t* values_1, uint16_t values_1_count,
                 const uint8_t* address_priorities_1, uint16_t address_priorities_1_count, uint8_t priority_2,
                 const uint8_t* values_2, uint16_t values_2_count, const uint8_t* address_priorities_2,
                 uint16_t address_priorities_2_count, MergeTestType merge_type)
  {
    // Initialize the merger and sources.
    sacn_source_id_t source_1;
    sacn_source_id_t source_2;
    EtcPalUuid source_1_cid = GenV5(1);
    EtcPalUuid source_2_cid = GenV5(2);

    EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_1_cid, &source_1), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_2_cid, &source_2), kEtcPalErrOk);

    // Define the expected merge results.
    uint8_t expected_winning_values[DMX_ADDRESS_COUNT];
    sacn_source_id_t expected_winning_sources[DMX_ADDRESS_COUNT];

    for (unsigned int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      bool source_1_is_sourced = (i < values_1_count) &&
                                 !((i < address_priorities_1_count) && (address_priorities_1[i] == 0)) &&
                                 !((i >= address_priorities_1_count) && (address_priorities_1_count > 0));
      bool source_2_is_sourced =
          (i < values_2_count) && ((merge_type == MergeTestType::kStopSourcePap) ||  // If kStopSourcePap, exclude PAPs.
                                   (!((i < address_priorities_2_count) && (address_priorities_2[i] == 0)) &&
                                    !((i >= address_priorities_2_count) && (address_priorities_2_count > 0))));

      // These priorities and values are only valid if the corresponding source is sourcing at i.
      int current_priority_1 = (i < address_priorities_1_count) ? address_priorities_1[i] : priority_1;
      // If kStopSourcePap is used, then filter out the PAPs of the second source.
      int current_priority_2 = ((i < address_priorities_2_count) && (merge_type != MergeTestType::kStopSourcePap))
                                   ? address_priorities_2[i]
                                   : priority_2;
      int current_value_1 = (i < values_1_count) ? values_1[i] : -1;
      int current_value_2 = (i < values_2_count) ? values_2[i] : -1;

      if (source_1_is_sourced && (!source_2_is_sourced || (current_priority_1 > current_priority_2) ||
                                  ((current_priority_1 == current_priority_2) && (current_value_1 > current_value_2))))
      {
        expected_winning_values[i] = values_1[i];
        expected_winning_sources[i] = source_1;
      }
      else if (source_2_is_sourced)
      {
        expected_winning_values[i] = values_2[i];
        expected_winning_sources[i] = source_2;
      }
      else
      {
        expected_winning_sources[i] = SACN_DMX_MERGER_SOURCE_INVALID;
      }
    }

    // Make the merge calls.
    if (merge_type == MergeTestType::kUpdateSourceFromSacn)
    {
      SacnHeaderData header_1 = header_default_;
      GenV5(1, &header_1.cid);
      header_1.priority = priority_1;

      if (values_1_count)
      {
        header_1.start_code = 0x00;
        header_1.slot_count = values_1_count;
        EXPECT_EQ(sacn_dmx_merger_update_source_from_sacn(merger_handle_, &header_1, values_1), kEtcPalErrOk);
      }

      if (address_priorities_1_count)
      {
        header_1.start_code = 0xDD;
        header_1.slot_count = address_priorities_1_count;
        EXPECT_EQ(sacn_dmx_merger_update_source_from_sacn(merger_handle_, &header_1, address_priorities_1),
                  kEtcPalErrOk);
      }

      SacnHeaderData header_2 = header_default_;
      GenV5(2, &header_2.cid);
      header_2.priority = priority_2;

      if (values_2_count)
      {
        header_2.start_code = 0x00;
        header_2.slot_count = values_2_count;
        EXPECT_EQ(sacn_dmx_merger_update_source_from_sacn(merger_handle_, &header_2, values_2), kEtcPalErrOk);
      }

      if (address_priorities_2_count)
      {
        header_2.start_code = 0xDD;
        header_2.slot_count = address_priorities_2_count;
        EXPECT_EQ(sacn_dmx_merger_update_source_from_sacn(merger_handle_, &header_2, address_priorities_2),
                  kEtcPalErrOk);
      }
    }
    else if ((merge_type == MergeTestType::kUpdateSourceData) || (merge_type == MergeTestType::kStopSourcePap))
    {
      EXPECT_EQ(sacn_dmx_merger_update_source_data(merger_handle_, source_1, priority_1, values_1, values_1_count,
                                                   address_priorities_1, address_priorities_1_count),
                kEtcPalErrOk);
      EXPECT_EQ(sacn_dmx_merger_update_source_data(merger_handle_, source_2, priority_2, values_2, values_2_count,
                                                   address_priorities_2, address_priorities_2_count),
                kEtcPalErrOk);
    }

    // Execute stop_source_per_address_priority if needed.
    if (merge_type == MergeTestType::kStopSourcePap)
    {
      EXPECT_EQ(sacn_dmx_merger_stop_source_per_address_priority(merger_handle_, source_2), kEtcPalErrOk);
    }

    // Verify the merge results.
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      EXPECT_EQ(merger_config_.slot_owners[i], expected_winning_sources[i]) << "Test failed on iteration " << i << ".";

      if (expected_winning_sources[i] != SACN_DMX_MERGER_SOURCE_INVALID)
        EXPECT_EQ(merger_config_.slots[i], expected_winning_values[i]) << "Test failed on iteration " << i << ".";
    }

    // Deinitialize the sources and merger.
    EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_1), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_2), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);
  }

  void TestMerge(uint8_t priority_1, const uint8_t* values_1, const uint8_t* address_priorities_1, uint8_t priority_2,
                 const uint8_t* values_2, const uint8_t* address_priorities_2, MergeTestType merge_type)
  {
    TestMerge(priority_1, values_1, values_1 ? DMX_ADDRESS_COUNT : 0, address_priorities_1,
              address_priorities_1 ? DMX_ADDRESS_COUNT : 0, priority_2, values_2, values_2 ? DMX_ADDRESS_COUNT : 0,
              address_priorities_2, address_priorities_2 ? DMX_ADDRESS_COUNT : 0, merge_type);
  }

  void TestAddSourceMemLimit(bool infinite)
  {
    // Initialize a merger.
    merger_config_.source_count_max =
        infinite ? SACN_RECEIVER_INFINITE_SOURCES : SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER;
    EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

    // Add up to the maximum number of sources.
    EtcPalUuid source_cid;
    sacn_source_id_t source_handle;

    for (int i = 0; i < SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER; ++i)
    {
      GenV5(i, &source_cid);
      EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle), kEtcPalErrOk);
    }

    // Now add one more source.
    GenV5(SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER, &source_cid);

#if SACN_DYNAMIC_MEM
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle),
              infinite ? kEtcPalErrOk : kEtcPalErrNoMem);
#else
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle), kEtcPalErrNoMem);
#endif

    EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);
  }

  SacnHeaderData header_default_;
  uint8_t pdata_default_[DMX_ADDRESS_COUNT];

  uint8_t slots_[DMX_ADDRESS_COUNT];
  sacn_source_id_t slot_owners_[DMX_ADDRESS_COUNT];
  sacn_dmx_merger_t merger_handle_;
  SacnDmxMergerConfig merger_config_;

  uint8_t test_values_ascending_[DMX_ADDRESS_COUNT];
  uint8_t test_values_descending_[DMX_ADDRESS_COUNT];

  EtcPalUuid namespace_uuid_;
};

TEST_F(TestDmxMerger, DeinitClearsMergers)
{
  // Add up to the maximum number of mergers.
  for (int i = 0; i < SACN_DMX_MERGER_MAX_MERGERS; ++i)
    EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  EXPECT_EQ(get_number_of_mergers(), static_cast<size_t>(SACN_DMX_MERGER_MAX_MERGERS));

  sacn_dmx_merger_deinit();

  EXPECT_EQ(get_number_of_mergers(), 0u);
}

TEST_F(TestDmxMerger, MergerCreateWorks)
{
  // Initialize the initial values, and what we expect them to be after sacn_dmx_merger_create.
  uint8_t expected_slots_priorities[DMX_ADDRESS_COUNT];
  sacn_source_id_t expected_slot_owners[DMX_ADDRESS_COUNT];

  for (uint16_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    slots_[i] = i % 0xff;
    slot_owners_[i] = i;
    expected_slots_priorities[i] = 0;
    expected_slot_owners[i] = SACN_DMX_MERGER_SOURCE_INVALID;
  }

  // Start with a value that the merger handle will not end up being.
  sacn_dmx_merger_t initial_handle = 1234567;
  merger_handle_ = initial_handle;

  // Expect no merger states initially.
  EXPECT_EQ(get_number_of_mergers(), 0u);

  // Call sacn_dmx_merger_create and make sure it indicates success.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Make sure the values changed as expected.
  EXPECT_NE(merger_handle_, initial_handle);
  EXPECT_EQ(memcmp(slots_, expected_slots_priorities, DMX_ADDRESS_COUNT), 0);
  EXPECT_EQ(memcmp(slot_owners_, expected_slot_owners, sizeof(sacn_source_id_t) * DMX_ADDRESS_COUNT), 0);

  // Make sure the correct merger state was created.
  EXPECT_EQ(get_number_of_mergers(), 1u);

  MergerState* merger_state = find_merger_state(merger_handle_);
  ASSERT_NE(merger_state, nullptr);

  EXPECT_EQ(merger_state->handle, merger_handle_);
  EXPECT_EQ(merger_state->source_count_max, merger_config_.source_count_max);
  EXPECT_EQ(merger_state->slots, merger_config_.slots);
  EXPECT_EQ(merger_state->slot_owners, merger_config_.slot_owners);
  EXPECT_EQ(memcmp(merger_state->winning_priorities, expected_slots_priorities, DMX_ADDRESS_COUNT), 0);
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_handle_lookup), 0u);
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 0u);
}

TEST_F(TestDmxMerger, MergerCreateErrInvalidWorks)
{
  SacnDmxMergerConfig invalidSlotsConfig = merger_config_;
  invalidSlotsConfig.slots = NULL;

  SacnDmxMergerConfig invalidSlotOwnersConfig = merger_config_;
  invalidSlotOwnersConfig.slot_owners = NULL;

  etcpal_error_t null_config_result = sacn_dmx_merger_create(NULL, &merger_handle_);
  etcpal_error_t null_handle_result = sacn_dmx_merger_create(&merger_config_, NULL);
  etcpal_error_t null_slots_result = sacn_dmx_merger_create(&invalidSlotsConfig, &merger_handle_);
  etcpal_error_t null_slot_owners_result = sacn_dmx_merger_create(&invalidSlotOwnersConfig, &merger_handle_);

  etcpal_error_t valid_result = sacn_dmx_merger_create(&merger_config_, &merger_handle_);

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

TEST_F(TestDmxMerger, MergerCreateErrNoMemWorks)
{
  // Add up to the maximum number of mergers.
  for (int i = 0; i < SACN_DMX_MERGER_MAX_MERGERS; ++i)
    EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Add one more merger, which should only fail with static memory.
  etcpal_error_t past_max_result = sacn_dmx_merger_create(&merger_config_, &merger_handle_);

#if SACN_DYNAMIC_MEM
  EXPECT_EQ(past_max_result, kEtcPalErrOk);
#else
  EXPECT_EQ(past_max_result, kEtcPalErrNoMem);
#endif
}

TEST_F(TestDmxMerger, MergerDestroyWorks)
{
  EXPECT_EQ(get_number_of_mergers(), 0u);
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);
  EXPECT_EQ(find_merger_state(merger_handle_), nullptr);
  EXPECT_EQ(get_number_of_mergers(), 0u);
}

TEST_F(TestDmxMerger, MergerDestroyErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_destroy(0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_destroy(0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, MergerDestroyErrNotFoundWorks)
{
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  EXPECT_EQ(sacn_dmx_merger_destroy(SACN_DMX_MERGER_INVALID), kEtcPalErrNotFound);
  EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrNotFound);
}

TEST_F(TestDmxMerger, AddSourceWorks)
{
  // Create the merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Add the source, and verify success.
  EtcPalUuid source_cid;
  GenV5(0, &source_cid);

  sacn_source_id_t source_handle = SACN_DMX_MERGER_SOURCE_INVALID;

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle), kEtcPalErrOk);

  // Make sure the handle was updated.
  EXPECT_NE(source_handle, SACN_DMX_MERGER_SOURCE_INVALID);

  // Grab the merger state.
  MergerState* merger_state = find_merger_state(merger_handle_);
  ASSERT_NE(merger_state, nullptr);

  // Check the CID-to-handle mapping first.
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_handle_lookup), 1u);

  CidHandleMapping* cid_handle_mapping =
      reinterpret_cast<CidHandleMapping*>(etcpal_rbtree_find(&merger_state->source_handle_lookup, &source_cid));
  ASSERT_NE(cid_handle_mapping, nullptr);

  EXPECT_EQ(memcmp(cid_handle_mapping->cid.data, source_cid.data, ETCPAL_UUID_BYTES), 0);
  EXPECT_EQ(cid_handle_mapping->handle, source_handle);

  // Now check the source state.
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 1u);

  SourceState* source_state =
      reinterpret_cast<SourceState*>(etcpal_rbtree_find(&merger_state->source_state_lookup, &source_handle));
  ASSERT_NE(source_state, nullptr);

  EXPECT_EQ(source_state->handle, source_handle);
  EXPECT_EQ(memcmp(source_state->source.cid.data, source_cid.data, ETCPAL_UUID_BYTES), 0);
  EXPECT_EQ(source_state->source.valid_value_count, 0u);
  EXPECT_EQ(source_state->source.universe_priority, 0u);
  EXPECT_EQ(source_state->source.address_priority_valid, false);

  uint8_t expected_values_priorities[DMX_ADDRESS_COUNT];
  memset(expected_values_priorities, 0, DMX_ADDRESS_COUNT);

  EXPECT_EQ(memcmp(source_state->source.values, expected_values_priorities, DMX_ADDRESS_COUNT), 0);
  EXPECT_EQ(memcmp(source_state->source.address_priority, expected_values_priorities, DMX_ADDRESS_COUNT), 0);
}

TEST_F(TestDmxMerger, AddSourceErrInvalidWorks)
{
  // Initialize a merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Run tests.
  EtcPalUuid source_cid;
  sacn_source_id_t source_handle;

  etcpal_error_t null_cid_result = sacn_dmx_merger_add_source(merger_handle_, NULL, &source_handle);
  etcpal_error_t null_source_handle_result = sacn_dmx_merger_add_source(merger_handle_, &source_cid, NULL);
  etcpal_error_t unknown_merger_handle_result =
      sacn_dmx_merger_add_source(merger_handle_ + 1, &source_cid, &source_handle);
  etcpal_error_t invalid_merger_handle_result =
      sacn_dmx_merger_add_source(SACN_DMX_MERGER_INVALID, &source_cid, &source_handle);

  etcpal_error_t valid_result = sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle);

  EXPECT_EQ(null_cid_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_source_handle_result, kEtcPalErrInvalid);
  EXPECT_EQ(unknown_merger_handle_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_merger_handle_result, kEtcPalErrInvalid);

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
  TestAddSourceMemLimit(false);
  TestAddSourceMemLimit(true);
}

TEST_F(TestDmxMerger, AddSourceErrExistsWorks)
{
  // Initialize a merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Initialize a source.
  const char* cid_str_1 = "1234567890abcdef";

  EtcPalUuid source_cid_1;
  sacn_source_id_t source_handle_1;

  memcpy(source_cid_1.data, cid_str_1, ETCPAL_UUID_BYTES);

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_1, &source_handle_1), kEtcPalErrOk);

  // Try to add another source with the same CID.
  EtcPalUuid source_cid_2;
  sacn_source_id_t source_handle_2;

  memcpy(source_cid_2.data, cid_str_1, ETCPAL_UUID_BYTES);

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_2, &source_handle_2), kEtcPalErrExists);

  // Try to add another source with a different CID.
  const char* cid_str_2 = "abcdef1234567890";

  EtcPalUuid source_cid_3;
  sacn_source_id_t source_handle_3;

  memcpy(source_cid_3.data, cid_str_2, ETCPAL_UUID_BYTES);

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_3, &source_handle_3), kEtcPalErrOk);
}

TEST_F(TestDmxMerger, RemoveSourceUpdatesMergeOutput)
{
  // Create the merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Grab the merger state, which will be used later.
  MergerState* merger_state = find_merger_state(merger_handle_);
  ASSERT_NE(merger_state, nullptr);

  // Add a couple of sources.
  EtcPalUuid source_1_cid;
  EtcPalUuid source_2_cid;
  GenV5(0, &source_1_cid);
  GenV5(1, &source_2_cid);

  sacn_source_id_t source_1_handle;
  sacn_source_id_t source_2_handle;

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_1_cid, &source_1_handle), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_2_cid, &source_2_handle), kEtcPalErrOk);

  // Make constants for source data about to be fed in.
  const uint8_t source_1_value = 50;
  const uint8_t source_2_value = 70;
  const uint8_t source_1_priority = 128;
  const uint8_t source_2_priority_1 = 1;    // This should be less than source_1_priority.
  const uint8_t source_2_priority_2 = 255;  // This should be greater than source_1_priority.

  // Feed in data from source 1 with a universe priority.
  uint8_t priority = source_1_priority;
  uint8_t values[DMX_ADDRESS_COUNT];
  memset(values, source_1_value, DMX_ADDRESS_COUNT);

  sacn_dmx_merger_update_source_data(merger_handle_, source_1_handle, priority, values, DMX_ADDRESS_COUNT, nullptr, 0);

  // Feed in data from source 2 with per-address-priorities, one half lower and one half higher.
  uint8_t priorities[DMX_ADDRESS_COUNT];
  memset(priorities, source_2_priority_1, DMX_ADDRESS_COUNT / 2);
  memset(&priorities[DMX_ADDRESS_COUNT / 2], source_2_priority_2, DMX_ADDRESS_COUNT / 2);

  memset(values, source_2_value, DMX_ADDRESS_COUNT);

  sacn_dmx_merger_update_source_data(merger_handle_, source_2_handle, 0, values, DMX_ADDRESS_COUNT, priorities,
                                     DMX_ADDRESS_COUNT);

  // Before removing a source, check the output.
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    if (i < (DMX_ADDRESS_COUNT / 2))
    {
      EXPECT_EQ(merger_config_.slots[i], source_1_value);
      EXPECT_EQ(merger_config_.slot_owners[i], source_1_handle);
    }
    else
    {
      EXPECT_EQ(merger_config_.slots[i], source_2_value);
      EXPECT_EQ(merger_config_.slot_owners[i], source_2_handle);
    }
  }

  // Now remove source 2 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_2_handle), kEtcPalErrOk);

  // The output should be just source 1 now.
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    EXPECT_EQ(merger_config_.slots[i], source_1_value);
    EXPECT_EQ(merger_config_.slot_owners[i], source_1_handle);
  }

  // Now remove source 1 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_1_handle), kEtcPalErrOk);

  // The output should indicate that no slots are being sourced.
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    EXPECT_EQ(merger_config_.slot_owners[i], SACN_DMX_MERGER_SOURCE_INVALID);
}

TEST_F(TestDmxMerger, RemoveSourceUpdatesInternalState)
{
  // Create the merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Grab the merger state, which will be used later.
  MergerState* merger_state = find_merger_state(merger_handle_);
  ASSERT_NE(merger_state, nullptr);

  // Add a couple of sources.
  EtcPalUuid source_1_cid;
  EtcPalUuid source_2_cid;
  GenV5(0, &source_1_cid);
  GenV5(1, &source_2_cid);

  sacn_source_id_t source_1_handle;
  sacn_source_id_t source_2_handle;

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_1_cid, &source_1_handle), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_2_cid, &source_2_handle), kEtcPalErrOk);

  // Each tree should have a size of 2.
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_handle_lookup), 2u);
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 2u);

  // Remove source 1 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_1_handle), kEtcPalErrOk);

  // Each tree should have a size of 1.
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_handle_lookup), 1u);
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 1u);

  // Remove source 2 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_2_handle), kEtcPalErrOk);

  // Each tree should have a size of 0.
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_handle_lookup), 0u);
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 0u);
}

TEST_F(TestDmxMerger, RemoveSourceErrInvalidWorks)
{
  // Create merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Test response to SACN_DMX_MERGER_SOURCE_INVALID.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID), kEtcPalErrInvalid);

  // Add a source.
  EtcPalUuid source_cid;
  sacn_source_id_t source_handle;
  memcpy(source_cid.data, "1234567890abcdef", ETCPAL_UUID_BYTES);
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle), kEtcPalErrOk);

  // Test response to SACN_DMX_MERGER_INVALID.
  EXPECT_EQ(sacn_dmx_merger_remove_source(SACN_DMX_MERGER_INVALID, source_handle), kEtcPalErrInvalid);

  // The first removal should succeed, but the second should fail because the source is no longer there.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_handle), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_handle), kEtcPalErrInvalid);

  // Add the source again.
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid, &source_handle), kEtcPalErrOk);

  // This time remove the merger.
  EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);

  // Now the source removal should fail because the merger cannot be found.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_handle), kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, RemoveSourceErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_remove_source(0, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_remove_source(0, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, GetIdWorks)
{
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  const char* cid_str_1 = "abcdef1234567890";
  const char* cid_str_2 = "1234567890abcdef";

  sacn_source_id_t source_handle;

  EtcPalUuid source_cid_1;
  EtcPalUuid source_cid_2;

  memcpy(source_cid_1.data, cid_str_1, ETCPAL_UUID_BYTES);
  memcpy(source_cid_2.data, cid_str_2, ETCPAL_UUID_BYTES);

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_1, &source_handle), kEtcPalErrOk);

  EXPECT_EQ(sacn_dmx_merger_get_id(merger_handle_, NULL), SACN_DMX_MERGER_SOURCE_INVALID);
  EXPECT_EQ(sacn_dmx_merger_get_id(SACN_DMX_MERGER_INVALID, &source_cid_1), SACN_DMX_MERGER_SOURCE_INVALID);
  EXPECT_EQ(sacn_dmx_merger_get_id(merger_handle_ + 1, &source_cid_1), SACN_DMX_MERGER_SOURCE_INVALID);
  EXPECT_EQ(sacn_dmx_merger_get_id(merger_handle_, &source_cid_2), SACN_DMX_MERGER_SOURCE_INVALID);
  EXPECT_EQ(sacn_dmx_merger_get_id(merger_handle_, &source_cid_1), source_handle);

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_2, &source_handle), kEtcPalErrOk);

  EXPECT_EQ(sacn_dmx_merger_get_id(merger_handle_, &source_cid_2), source_handle);
}

TEST_F(TestDmxMerger, GetSourceWorks)
{
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  const char* cid_str_1 = "abcdef1234567890";
  const char* cid_str_2 = "1234567890abcdef";

  EtcPalUuid source_cid_1;
  EtcPalUuid source_cid_2;

  memcpy(source_cid_1.data, cid_str_1, ETCPAL_UUID_BYTES);
  memcpy(source_cid_2.data, cid_str_2, ETCPAL_UUID_BYTES);

  sacn_source_id_t source_handle_1;
  sacn_source_id_t source_handle_2;

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_1, &source_handle_1), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_cid_2, &source_handle_2), kEtcPalErrOk);

  EXPECT_EQ(sacn_dmx_merger_get_source(SACN_DMX_MERGER_INVALID, source_handle_1), nullptr);
  EXPECT_EQ(sacn_dmx_merger_get_source(merger_handle_ + 1, source_handle_1), nullptr);
  EXPECT_EQ(sacn_dmx_merger_get_source(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID), nullptr);
  EXPECT_EQ(sacn_dmx_merger_get_source(merger_handle_, source_handle_2 + 1), nullptr);

  const SacnDmxMergerSource* source_1 = sacn_dmx_merger_get_source(merger_handle_, source_handle_1);
  const SacnDmxMergerSource* source_2 = sacn_dmx_merger_get_source(merger_handle_, source_handle_2);

  ASSERT_NE(source_1, nullptr);
  ASSERT_NE(source_2, nullptr);

  EXPECT_EQ(memcmp(source_1->cid.data, source_cid_1.data, ETCPAL_UUID_BYTES), 0);
  EXPECT_EQ(memcmp(source_2->cid.data, source_cid_2.data, ETCPAL_UUID_BYTES), 0);
}

TEST_F(TestDmxMerger, UpdateSourceDataMergesLevels)
{
  TestMerge(100, test_values_ascending_, nullptr, 100, test_values_descending_, nullptr,
            MergeTestType::kUpdateSourceData);
}

TEST_F(TestDmxMerger, UpdateSourceDataMergesPaps)
{
  TestMerge(100, test_values_ascending_, test_values_descending_, 100, test_values_descending_, test_values_ascending_,
            MergeTestType::kUpdateSourceData);
}

TEST_F(TestDmxMerger, UpdateSourceDataMergesUps)
{
  TestMerge(0, test_values_ascending_, nullptr, 0, test_values_descending_, nullptr, MergeTestType::kUpdateSourceData);
  TestMerge(0, test_values_ascending_, nullptr, 200, test_values_descending_, nullptr,
            MergeTestType::kUpdateSourceData);
  TestMerge(200, test_values_ascending_, nullptr, 0, test_values_descending_, nullptr,
            MergeTestType::kUpdateSourceData);
}

TEST_F(TestDmxMerger, UpdateSourceDataMergesPapsWithUps)
{
  TestMerge(100, test_values_ascending_, test_values_descending_, 100, test_values_descending_, nullptr,
            MergeTestType::kUpdateSourceData);
}

TEST_F(TestDmxMerger, UpdateSourceDataMergesUpsWithPaps)
{
  TestMerge(100, test_values_ascending_, nullptr, 100, test_values_descending_, test_values_ascending_,
            MergeTestType::kUpdateSourceData);
}

TEST_F(TestDmxMerger, UpdateSourceDataHandlesValidValueCount)
{
  for (uint16_t i = 1; i <= DMX_ADDRESS_COUNT; ++i)
  {
    TestMerge(100, test_values_ascending_, DMX_ADDRESS_COUNT, nullptr, 0, 100, test_values_descending_, i, nullptr, 0,
              MergeTestType::kUpdateSourceData);
  }
}

TEST_F(TestDmxMerger, UpdateSourceDataHandlesLessPaps)
{
  for (uint16_t i = 1; i < DMX_ADDRESS_COUNT; ++i)
  {
    TestMerge(100, test_values_ascending_, DMX_ADDRESS_COUNT, test_values_descending_, DMX_ADDRESS_COUNT, 100,
              test_values_descending_, DMX_ADDRESS_COUNT, test_values_ascending_, i, MergeTestType::kUpdateSourceData);
  }
}

TEST_F(TestDmxMerger, UpdateSourceDataErrInvalidWorks)
{
  uint8_t foo = 0;

  etcpal_error_t invalid_merger_result =
      sacn_dmx_merger_update_source_data(SACN_DMX_MERGER_INVALID, 0, VALID_PRIORITY, nullptr, 0, nullptr, 0);
  etcpal_error_t invalid_source_result =
      sacn_dmx_merger_update_source_data(0, SACN_DMX_MERGER_SOURCE_INVALID, VALID_PRIORITY, nullptr, 0, nullptr, 0);
  etcpal_error_t invalid_new_values_result =
      sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, &foo, 0, nullptr, 0);
  etcpal_error_t invalid_new_values_count_result_1 =
      sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, nullptr, 1, nullptr, 0);
  etcpal_error_t invalid_new_values_count_result_2 =
      sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, &foo, DMX_ADDRESS_COUNT + 1, nullptr, 0);
  etcpal_error_t invalid_priority_result =
      sacn_dmx_merger_update_source_data(0, 0, INVALID_PRIORITY, nullptr, 0, nullptr, 0);
  etcpal_error_t invalid_address_priorities_result =
      sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, nullptr, 0, &foo, 0);
  etcpal_error_t invalid_address_priorities_count_result_1 =
      sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, nullptr, 0, nullptr, 1);
  etcpal_error_t invalid_address_priorities_count_result_2 =
      sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, nullptr, 0, &foo, DMX_ADDRESS_COUNT + 1);

  etcpal_error_t valid_result_1 = sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, nullptr, 0, nullptr, 0);
  etcpal_error_t valid_result_2 = sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, &foo, 1, nullptr, 0);
  etcpal_error_t valid_result_3 = sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, nullptr, 0, &foo, 1);
  etcpal_error_t valid_result_4 = sacn_dmx_merger_update_source_data(0, 0, VALID_PRIORITY, &foo, 1, &foo, 1);

  EXPECT_EQ(invalid_merger_result, kEtcPalErrInvalid);
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
  etcpal_error_t not_initialized_result = sacn_dmx_merger_update_source_data(0, 0, 0, nullptr, 0, nullptr, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_update_source_data(0, 0, 0, nullptr, 0, nullptr, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, UpdateSourceDataErrNotFoundWorks)
{
  sacn_source_id_t source = 0;

  etcpal_error_t no_merger_result =
      sacn_dmx_merger_update_source_data(merger_handle_, source, VALID_PRIORITY, nullptr, 0, nullptr, 0);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result =
      sacn_dmx_merger_update_source_data(merger_handle_, source, VALID_PRIORITY, nullptr, 0, nullptr, 0);

  sacn_dmx_merger_add_source(merger_handle_, &header_default_.cid, &source);

  etcpal_error_t found_result =
      sacn_dmx_merger_update_source_data(merger_handle_, source, VALID_PRIORITY, nullptr, 0, nullptr, 0);

  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnMergesLevels)
{
  TestMerge(100, test_values_ascending_, nullptr, 100, test_values_descending_, nullptr,
            MergeTestType::kUpdateSourceFromSacn);
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnMergesPaps)
{
  TestMerge(100, test_values_ascending_, test_values_descending_, 100, test_values_descending_, test_values_ascending_,
            MergeTestType::kUpdateSourceFromSacn);
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnMergesUps)
{
  TestMerge(0, test_values_ascending_, nullptr, 0, test_values_descending_, nullptr,
            MergeTestType::kUpdateSourceFromSacn);
  TestMerge(0, test_values_ascending_, nullptr, 200, test_values_descending_, nullptr,
            MergeTestType::kUpdateSourceFromSacn);
  TestMerge(200, test_values_ascending_, nullptr, 0, test_values_descending_, nullptr,
            MergeTestType::kUpdateSourceFromSacn);
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnMergesPapsWithUps)
{
  TestMerge(100, test_values_ascending_, test_values_descending_, 100, test_values_descending_, nullptr,
            MergeTestType::kUpdateSourceFromSacn);
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnMergesUpsWithPaps)
{
  TestMerge(100, test_values_ascending_, nullptr, 100, test_values_descending_, test_values_ascending_,
            MergeTestType::kUpdateSourceFromSacn);
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnHandlesValidValueCount)
{
  for (uint16_t i = 1; i <= DMX_ADDRESS_COUNT; ++i)
  {
    TestMerge(100, test_values_ascending_, DMX_ADDRESS_COUNT, nullptr, 0, 100, test_values_descending_, i, nullptr, 0,
              MergeTestType::kUpdateSourceFromSacn);
  }
}

TEST_F(TestDmxMerger, UpdateSourceFromSacnHandlesLessPaps)
{
  for (uint16_t i = 1; i < DMX_ADDRESS_COUNT; ++i)
  {
    TestMerge(100, test_values_ascending_, DMX_ADDRESS_COUNT, test_values_descending_, DMX_ADDRESS_COUNT, 100,
              test_values_descending_, DMX_ADDRESS_COUNT, test_values_ascending_, i,
              MergeTestType::kUpdateSourceFromSacn);
  }
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

  etcpal_error_t invalid_merger_result =
      sacn_dmx_merger_update_source_from_sacn(SACN_DMX_MERGER_INVALID, &header_default_, pdata_default_);
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

  EXPECT_EQ(invalid_merger_result, kEtcPalErrInvalid);
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
  sacn_source_id_t source;
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

TEST_F(TestDmxMerger, StopSourcePapWorks)
{
  TestMerge(100, test_values_ascending_, test_values_descending_, 200, test_values_descending_, test_values_ascending_,
            MergeTestType::kStopSourcePap);
}

TEST_F(TestDmxMerger, StopSourcePapErrNotFoundWorks)
{
  sacn_source_id_t source = SACN_DMX_MERGER_SOURCE_INVALID;

  etcpal_error_t invalid_source_result = sacn_dmx_merger_stop_source_per_address_priority(merger_handle_, source);

  source = 1;

  etcpal_error_t no_merger_result = sacn_dmx_merger_stop_source_per_address_priority(merger_handle_, source);
  etcpal_error_t invalid_merger_result =
      sacn_dmx_merger_stop_source_per_address_priority(SACN_DMX_MERGER_INVALID, source);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result = sacn_dmx_merger_stop_source_per_address_priority(merger_handle_, source);

  sacn_dmx_merger_add_source(merger_handle_, &header_default_.cid, &source);

  etcpal_error_t found_result = sacn_dmx_merger_stop_source_per_address_priority(merger_handle_, source);

  EXPECT_EQ(invalid_source_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(invalid_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}

TEST_F(TestDmxMerger, StopSourcePapErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_stop_source_per_address_priority(0, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_stop_source_per_address_priority(0, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, SourceIsValidWorks)
{
  sacn_source_id_t slot_owners_array[DMX_ADDRESS_COUNT];
  memset(slot_owners_array, 1u, DMX_ADDRESS_COUNT);       // Fill with non-zero values.
  slot_owners_array[1] = SACN_DMX_MERGER_SOURCE_INVALID;  // Set one of them to invalid.

  EXPECT_EQ(SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners_array, 0), true);
  EXPECT_EQ(SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners_array, 1), false);
  EXPECT_EQ(SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners_array, 2), true);
}
