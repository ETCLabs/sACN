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

#include "sacn/dmx_merger.h"

#include <limits>
#include <optional>
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/source_loss.h"
#include "sacn_mock/private/sockets.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/dmx_merger.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestDmxMerger TestDmxMergerDynamic
#define TestDmxMergerUpdate TestDmxMergerUpdateDynamic
#else
#define TestDmxMerger TestDmxMergerStatic
#define TestDmxMergerUpdate TestDmxMergerUpdateStatic
#endif

constexpr uint8_t VALID_PRIORITY = 100;
constexpr uint8_t LOW_PRIORITY = 1;
constexpr uint8_t HIGH_PRIORITY = 200;

class TestDmxMerger : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_source_loss_reset_all_fakes();
    sacn_sockets_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_dmx_merger_init(), kEtcPalErrOk);

    merger_config_ = SACN_DMX_MERGER_CONFIG_INIT;
    merger_config_.slots = slots_;
    merger_config_.per_address_priorities = per_address_priorities_;
    merger_config_.slot_owners = slot_owners_;
    merger_config_.source_count_max = SACN_RECEIVER_INFINITE_SOURCES;
  }

  void TearDown() override
  {
    sacn_dmx_merger_deinit();
    sacn_mem_deinit();
  }

  void TestAddSourceMemLimit(bool infinite)
  {
    // Initialize a merger.
    merger_config_.source_count_max =
        infinite ? SACN_RECEIVER_INFINITE_SOURCES : SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER;
    EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

    // Add up to the maximum number of sources.
    sacn_dmx_merger_source_t source_handle;

    for (int i = 0; i < SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER; ++i)
    {
      EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle), kEtcPalErrOk);
    }

    // Now add one more source.
#if SACN_DYNAMIC_MEM
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle), infinite ? kEtcPalErrOk : kEtcPalErrNoMem);
#else
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle), kEtcPalErrNoMem);
#endif

    EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);
  }

  uint8_t slots_[DMX_ADDRESS_COUNT];
  uint8_t per_address_priorities_[DMX_ADDRESS_COUNT];
  sacn_dmx_merger_source_t slot_owners_[DMX_ADDRESS_COUNT];
  sacn_dmx_merger_t merger_handle_;
  SacnDmxMergerConfig merger_config_;
};

/* This fixture is for testing the update (merging) functions of the merger API. */
class TestDmxMergerUpdate : public TestDmxMerger
{
protected:
  void SetUp() override
  {
    TestDmxMerger::SetUp();

    // Initialize the merger and sources.
    EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &merge_source_1_), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &merge_source_2_), kEtcPalErrOk);

    // Initialize expected merge results.
    ClearExpectedMergeResults();
  }

  void TearDown() override
  {
    // Deinitialize the sources and merger.
    EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_1_), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);

    TestDmxMerger::TearDown();
  }

  void UpdateExpectedMergeResults(sacn_dmx_merger_source_t source, std::optional<int> priority,
                                  std::optional<std::vector<uint8_t>> levels, std::optional<std::vector<uint8_t>> paps)
  {
    for (size_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      // If this slot is being sourced:
      if (levels && (i < levels->size()) && ((!paps && priority) || (paps && (i < paps->size()) && (paps->at(i) != 0))))
      {
        // Determine the current level and priority for this slot given it is being sourced.
        int current_level = (levels && (i < levels->size())) ? levels->at(i) : -1;
        int current_priority = (paps && (i < paps->size())) ? paps->at(i) : priority.value_or(0);

        // If this beats the current expected values, update the expected values.
        if ((expected_merge_winners_[i] == SACN_DMX_MERGER_SOURCE_INVALID) ||
            (current_priority > expected_merge_priorities_[i]) ||
            ((current_priority == expected_merge_priorities_[i]) && (current_level > expected_merge_levels_[i])))
        {
          expected_merge_levels_[i] = current_level;
          expected_merge_priorities_[i] = current_priority;
          expected_merge_winners_[i] = source;
        }
      }
    }
  }

  void VerifyMergeResults()
  {
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      uint8_t expected_slot = static_cast<uint8_t>(expected_merge_levels_[i]);
      uint8_t expected_pap =
          (expected_merge_winners_[i] == SACN_DMX_MERGER_SOURCE_INVALID)
              ? 0u
              : ((expected_merge_priorities_[i] == 0) ? 1u : static_cast<uint8_t>(expected_merge_priorities_[i]));

      EXPECT_EQ(merger_config_.slots[i], expected_slot) << "Test failed on iteration " << i << ".";
      EXPECT_EQ(merger_config_.per_address_priorities[i], expected_pap) << "Test failed on iteration " << i << ".";
      EXPECT_EQ(merger_config_.slot_owners[i], expected_merge_winners_[i]) << "Test failed on iteration " << i << ".";
    }
  }

  void ClearExpectedMergeResults()
  {
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      expected_merge_levels_[i] = 0;
      expected_merge_priorities_[i] = 0;
      expected_merge_winners_[i] = SACN_DMX_MERGER_SOURCE_INVALID;
    }
  }

  sacn_dmx_merger_source_t merge_source_1_;
  sacn_dmx_merger_source_t merge_source_2_;
  int expected_merge_levels_[DMX_ADDRESS_COUNT];
  int expected_merge_priorities_[DMX_ADDRESS_COUNT];
  sacn_dmx_merger_source_t expected_merge_winners_[DMX_ADDRESS_COUNT];

  const std::vector<uint8_t> test_values_ascending_ = [&] {
    std::vector<uint8_t> vect(DMX_ADDRESS_COUNT);
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
      vect[i] = i % 256;
    return vect;
  }();

  const std::vector<uint8_t> test_values_descending_ = [&] {
    std::vector<uint8_t> vect(DMX_ADDRESS_COUNT);
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
      vect[i] = 255 - (i % 256);
    return vect;
  }();
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
  sacn_dmx_merger_source_t expected_slot_owners[DMX_ADDRESS_COUNT];

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
  EXPECT_EQ(memcmp(per_address_priorities_, expected_slots_priorities, DMX_ADDRESS_COUNT), 0);
  EXPECT_EQ(memcmp(slot_owners_, expected_slot_owners, sizeof(sacn_dmx_merger_source_t) * DMX_ADDRESS_COUNT), 0);

  // Make sure the correct merger state was created.
  EXPECT_EQ(get_number_of_mergers(), 1u);

  MergerState* merger_state = nullptr;
  lookup_state(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID, &merger_state, nullptr);
  ASSERT_NE(merger_state, nullptr);

  EXPECT_EQ(merger_state->handle, merger_handle_);
  EXPECT_EQ(merger_state->config.source_count_max, merger_config_.source_count_max);
  EXPECT_EQ(merger_state->config.slots, merger_config_.slots);
  EXPECT_EQ(merger_state->config.per_address_priorities, merger_config_.per_address_priorities);
  EXPECT_EQ(merger_state->config.slot_owners, merger_config_.slot_owners);
  EXPECT_EQ(memcmp(merger_state->winning_priorities, expected_slots_priorities, DMX_ADDRESS_COUNT), 0);
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 0u);
}

TEST_F(TestDmxMerger, MergerCreateErrInvalidWorks)
{
  SacnDmxMergerConfig invalidSlotsConfig = merger_config_;
  invalidSlotsConfig.slots = NULL;

  SacnDmxMergerConfig invalidPapsConfig = merger_config_;
  invalidPapsConfig.per_address_priorities = NULL;

  SacnDmxMergerConfig invalidSlotOwnersConfig = merger_config_;
  invalidSlotOwnersConfig.slot_owners = NULL;

  etcpal_error_t null_config_result = sacn_dmx_merger_create(NULL, &merger_handle_);
  etcpal_error_t null_handle_result = sacn_dmx_merger_create(&merger_config_, NULL);
  etcpal_error_t null_slots_result = sacn_dmx_merger_create(&invalidSlotsConfig, &merger_handle_);

  etcpal_error_t null_paps_result = sacn_dmx_merger_create(&invalidPapsConfig, &merger_handle_);
  etcpal_error_t null_slot_owners_result = sacn_dmx_merger_create(&invalidSlotOwnersConfig, &merger_handle_);
  etcpal_error_t non_null_result = sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  EXPECT_EQ(null_config_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_handle_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_slots_result, kEtcPalErrInvalid);

  EXPECT_NE(null_paps_result, kEtcPalErrInvalid);
  EXPECT_NE(null_slot_owners_result, kEtcPalErrInvalid);
  EXPECT_NE(non_null_result, kEtcPalErrInvalid);
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
  MergerState* merger_state = nullptr;
  lookup_state(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID, &merger_state, nullptr);
  EXPECT_EQ(merger_state, nullptr);
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
  sacn_dmx_merger_source_t source_handle = SACN_DMX_MERGER_SOURCE_INVALID;

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle), kEtcPalErrOk);

  // Make sure the handle was updated.
  EXPECT_NE(source_handle, SACN_DMX_MERGER_SOURCE_INVALID);

  // Grab the merger state.
  MergerState* merger_state = nullptr;
  lookup_state(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID, &merger_state, nullptr);
  ASSERT_NE(merger_state, nullptr);

  // Now check the source state.
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 1u);

  SourceState* source_state =
      reinterpret_cast<SourceState*>(etcpal_rbtree_find(&merger_state->source_state_lookup, &source_handle));
  ASSERT_NE(source_state, nullptr);

  EXPECT_EQ(source_state->handle, source_handle);
  EXPECT_EQ(source_state->source.valid_level_count, 0u);
  EXPECT_EQ(source_state->source.universe_priority, 0u);
  EXPECT_EQ(source_state->source.address_priority_valid, false);

  uint8_t expected_levels_priorities[DMX_ADDRESS_COUNT];
  memset(expected_levels_priorities, 0, DMX_ADDRESS_COUNT);

  EXPECT_EQ(memcmp(source_state->source.levels, expected_levels_priorities, DMX_ADDRESS_COUNT), 0);
  EXPECT_EQ(memcmp(source_state->source.address_priority, expected_levels_priorities, DMX_ADDRESS_COUNT), 0);
}

TEST_F(TestDmxMerger, AddSourceErrInvalidWorks)
{
  // Initialize a merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Run tests.
  sacn_dmx_merger_source_t source_handle;

  etcpal_error_t null_source_handle_result = sacn_dmx_merger_add_source(merger_handle_, NULL);
  etcpal_error_t unknown_merger_handle_result = sacn_dmx_merger_add_source(merger_handle_ + 1, &source_handle);
  etcpal_error_t invalid_merger_handle_result = sacn_dmx_merger_add_source(SACN_DMX_MERGER_INVALID, &source_handle);

  etcpal_error_t valid_result = sacn_dmx_merger_add_source(merger_handle_, &source_handle);

  EXPECT_EQ(null_source_handle_result, kEtcPalErrInvalid);
  EXPECT_EQ(unknown_merger_handle_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_merger_handle_result, kEtcPalErrInvalid);

  EXPECT_NE(valid_result, kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, AddSourceErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_add_source(0, NULL);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_add_source(0, NULL);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, AddSourceErrNoMemWorks)
{
  TestAddSourceMemLimit(false);
  TestAddSourceMemLimit(true);
}

TEST_F(TestDmxMerger, RemoveSourceUpdatesMergeOutput)
{
  // Create the merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Grab the merger state, which will be used later.
  MergerState* merger_state = nullptr;
  lookup_state(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID, &merger_state, nullptr);
  ASSERT_NE(merger_state, nullptr);

  // Add a couple of sources.
  sacn_dmx_merger_source_t source_1_handle;
  sacn_dmx_merger_source_t source_2_handle;

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_1_handle), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_2_handle), kEtcPalErrOk);

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

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, source_1_handle, values, DMX_ADDRESS_COUNT), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, source_1_handle, priority), kEtcPalErrOk);

  // Feed in data from source 2 with per-address-priorities, one half lower and one half higher.
  uint8_t priorities[DMX_ADDRESS_COUNT];
  memset(priorities, source_2_priority_1, DMX_ADDRESS_COUNT / 2);
  memset(&priorities[DMX_ADDRESS_COUNT / 2], source_2_priority_2, DMX_ADDRESS_COUNT / 2);

  memset(values, source_2_value, DMX_ADDRESS_COUNT);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, source_2_handle, values, DMX_ADDRESS_COUNT), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, source_2_handle, priorities, DMX_ADDRESS_COUNT), kEtcPalErrOk);

  // Before removing a source, check the output.
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    if (i < (DMX_ADDRESS_COUNT / 2))
    {
      EXPECT_EQ(merger_config_.slots[i], source_1_value);
      EXPECT_EQ(merger_config_.per_address_priorities[i], source_1_priority);
      EXPECT_EQ(merger_config_.slot_owners[i], source_1_handle);
    }
    else
    {
      EXPECT_EQ(merger_config_.slots[i], source_2_value);
      EXPECT_EQ(merger_config_.per_address_priorities[i], source_2_priority_2);
      EXPECT_EQ(merger_config_.slot_owners[i], source_2_handle);
    }
  }

  // Now remove source 2 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_2_handle), kEtcPalErrOk);

  // The output should be just source 1 now.
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    EXPECT_EQ(merger_config_.slots[i], source_1_value);
    EXPECT_EQ(merger_config_.per_address_priorities[i], source_1_priority);
    EXPECT_EQ(merger_config_.slot_owners[i], source_1_handle);
  }

  // Now remove source 1 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_1_handle), kEtcPalErrOk);

  // The output should indicate that no slots are being sourced.
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    EXPECT_EQ(merger_config_.slots[i], 0u);
    EXPECT_EQ(merger_config_.per_address_priorities[i], 0u);
    EXPECT_EQ(merger_config_.slot_owners[i], SACN_DMX_MERGER_SOURCE_INVALID);
  }
}

TEST_F(TestDmxMerger, RemoveSourceUpdatesInternalState)
{
  // Create the merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Grab the merger state, which will be used later.
  MergerState* merger_state = nullptr;
  lookup_state(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID, &merger_state, nullptr);
  ASSERT_NE(merger_state, nullptr);

  // Add a couple of sources.
  sacn_dmx_merger_source_t source_1_handle;
  sacn_dmx_merger_source_t source_2_handle;

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_1_handle), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_2_handle), kEtcPalErrOk);

  // Tree should have a size of 2.
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 2u);

  // Remove source 1 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_1_handle), kEtcPalErrOk);

  // Tree should have a size of 1.
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 1u);

  // Remove source 2 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_2_handle), kEtcPalErrOk);

  // Tree should have a size of 0.
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 0u);
}

TEST_F(TestDmxMerger, RemoveSourceErrInvalidWorks)
{
  // Create merger.
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  // Test response to SACN_DMX_MERGER_SOURCE_INVALID.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID), kEtcPalErrInvalid);

  // Add a source.
  sacn_dmx_merger_source_t source_handle;
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle), kEtcPalErrOk);

  // Test response to SACN_DMX_MERGER_INVALID.
  EXPECT_EQ(sacn_dmx_merger_remove_source(SACN_DMX_MERGER_INVALID, source_handle), kEtcPalErrInvalid);

  // The first removal should succeed, but the second should fail because the source is no longer there.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_handle), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_handle), kEtcPalErrInvalid);

  // Add the source again.
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle), kEtcPalErrOk);

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

TEST_F(TestDmxMerger, GetSourceWorks)
{
  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  sacn_dmx_merger_source_t source_handle_1;
  sacn_dmx_merger_source_t source_handle_2;

  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle_1), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle_2), kEtcPalErrOk);

  EXPECT_EQ(sacn_dmx_merger_get_source(SACN_DMX_MERGER_INVALID, source_handle_1), nullptr);
  EXPECT_EQ(sacn_dmx_merger_get_source(merger_handle_ + 1, source_handle_1), nullptr);
  EXPECT_EQ(sacn_dmx_merger_get_source(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID), nullptr);
  EXPECT_EQ(sacn_dmx_merger_get_source(merger_handle_, source_handle_2 + 1), nullptr);

  EXPECT_NE(sacn_dmx_merger_get_source(merger_handle_, source_handle_1), nullptr);
  EXPECT_NE(sacn_dmx_merger_get_source(merger_handle_, source_handle_2), nullptr);
}

TEST_F(TestDmxMergerUpdate, MergesLevelsPermutation1)
{
  UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, VALID_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, VALID_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesLevelsPermutation2)
{
  UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, VALID_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, VALID_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesLevelsPermutation3)
{
  UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, VALID_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, VALID_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesLevelsPermutation4)
{
  UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, VALID_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, VALID_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapsPermutation1)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, test_values_ascending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapsPermutation2)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, test_values_ascending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapsPermutation3)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, test_values_ascending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapsPermutation4)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, test_values_ascending_);

  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesUps1)
{
  UpdateExpectedMergeResults(merge_source_1_, LOW_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, LOW_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, LOW_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, LOW_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesUps2)
{
  UpdateExpectedMergeResults(merge_source_1_, LOW_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, HIGH_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, LOW_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, HIGH_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesUps3)
{
  UpdateExpectedMergeResults(merge_source_1_, HIGH_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, LOW_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, HIGH_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, LOW_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapsWithUps)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, VALID_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, VALID_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesUpsWithPaps)
{
  UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, test_values_ascending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, HandlesLevelCount)
{
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, VALID_PRIORITY), kEtcPalErrOk);

  std::vector<uint8_t> variable_levels;

  for (uint16_t i = 0; i < test_values_descending_.size(); ++i)
  {
    variable_levels.push_back(test_values_descending_[variable_levels.size()]);

    EXPECT_EQ(
        sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, variable_levels.data(), variable_levels.size()),
        kEtcPalErrOk);

    ClearExpectedMergeResults();
    UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);
    UpdateExpectedMergeResults(merge_source_2_, VALID_PRIORITY, variable_levels, std::nullopt);
    VerifyMergeResults();
  }
}

TEST_F(TestDmxMergerUpdate, HandlesLessPaps)
{
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);

  std::vector<uint8_t> variable_paps;

  for (uint16_t i = 0; i < test_values_ascending_.size(); ++i)
  {
    variable_paps.push_back(test_values_ascending_[variable_paps.size()]);

    EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, variable_paps.data(), variable_paps.size()),
              kEtcPalErrOk);

    ClearExpectedMergeResults();
    UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
    UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, variable_paps);
    VerifyMergeResults();
  }
}

TEST_F(TestDmxMergerUpdate, DoesNotMergeWithoutLevels1)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, std::nullopt, test_values_ascending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, std::nullopt, test_values_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, DoesNotMergeWithoutLevels2)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_ascending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, std::nullopt, test_values_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, DoesNotMergeWithoutLevels3)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, std::nullopt, test_values_ascending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_ascending_, test_values_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, DoesNotMergeWithoutUpOrPap1)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, DoesNotMergeWithoutUpOrPap2)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_ascending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, DoesNotMergeWithoutUpOrPap3)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_ascending_, test_values_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMerger, UpdateLevelsErrInvalidWorks)
{
  uint8_t foo = 0;

  etcpal_error_t invalid_merger_result = sacn_dmx_merger_update_levels(SACN_DMX_MERGER_INVALID, 0, &foo, 1);
  etcpal_error_t invalid_source_result = sacn_dmx_merger_update_levels(0, SACN_DMX_MERGER_SOURCE_INVALID, &foo, 1);
  etcpal_error_t invalid_new_levels_result_1 = sacn_dmx_merger_update_levels(0, 0, nullptr, 0);
  etcpal_error_t invalid_new_levels_result_2 = sacn_dmx_merger_update_levels(0, 0, &foo, 0);
  etcpal_error_t invalid_new_levels_result_3 = sacn_dmx_merger_update_levels(0, 0, nullptr, 1);
  etcpal_error_t invalid_new_levels_result_4 = sacn_dmx_merger_update_levels(0, 0, &foo, DMX_ADDRESS_COUNT + 1);

  etcpal_error_t valid_result = sacn_dmx_merger_update_levels(0, 0, &foo, 1);

  EXPECT_EQ(invalid_merger_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_source_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_new_levels_result_1, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_new_levels_result_2, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_new_levels_result_3, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_new_levels_result_4, kEtcPalErrInvalid);

  EXPECT_NE(valid_result, kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, UpdateLevelsErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_update_levels(0, 0, nullptr, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_update_levels(0, 0, nullptr, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, UpdateLevelsErrNotFoundWorks)
{
  uint8_t foo = 0;
  sacn_dmx_merger_source_t source = 0;

  etcpal_error_t no_merger_result = sacn_dmx_merger_update_levels(merger_handle_, source, &foo, 1);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result = sacn_dmx_merger_update_levels(merger_handle_, source, &foo, 1);

  sacn_dmx_merger_add_source(merger_handle_, &source);

  etcpal_error_t found_result = sacn_dmx_merger_update_levels(merger_handle_, source, &foo, 1);

  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}

TEST_F(TestDmxMerger, UpdatePapsErrInvalidWorks)
{
  uint8_t foo = 0;

  etcpal_error_t invalid_merger_result = sacn_dmx_merger_update_paps(SACN_DMX_MERGER_INVALID, 0, &foo, 1);
  etcpal_error_t invalid_source_result = sacn_dmx_merger_update_paps(0, SACN_DMX_MERGER_SOURCE_INVALID, &foo, 1);
  etcpal_error_t invalid_paps_result_1 = sacn_dmx_merger_update_paps(0, 0, nullptr, 0);
  etcpal_error_t invalid_paps_result_2 = sacn_dmx_merger_update_paps(0, 0, &foo, 0);
  etcpal_error_t invalid_paps_result_3 = sacn_dmx_merger_update_paps(0, 0, nullptr, 1);
  etcpal_error_t invalid_paps_result_4 = sacn_dmx_merger_update_paps(0, 0, &foo, DMX_ADDRESS_COUNT + 1);

  etcpal_error_t valid_result = sacn_dmx_merger_update_paps(0, 0, &foo, 1);

  EXPECT_EQ(invalid_merger_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_source_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_paps_result_1, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_paps_result_2, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_paps_result_3, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_paps_result_4, kEtcPalErrInvalid);

  EXPECT_NE(valid_result, kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, UpdatePapsErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_update_paps(0, 0, nullptr, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_update_paps(0, 0, nullptr, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, UpdatePapsErrNotFoundWorks)
{
  uint8_t foo = 0;
  sacn_dmx_merger_source_t source = 0;

  etcpal_error_t no_merger_result = sacn_dmx_merger_update_paps(merger_handle_, source, &foo, 1);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result = sacn_dmx_merger_update_paps(merger_handle_, source, &foo, 1);

  sacn_dmx_merger_add_source(merger_handle_, &source);

  etcpal_error_t found_result = sacn_dmx_merger_update_paps(merger_handle_, source, &foo, 1);

  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}

TEST_F(TestDmxMerger, UpdateUniversePriorityErrInvalidWorks)
{
  etcpal_error_t invalid_merger_result =
      sacn_dmx_merger_update_universe_priority(SACN_DMX_MERGER_INVALID, 0, VALID_PRIORITY);
  etcpal_error_t invalid_source_result =
      sacn_dmx_merger_update_universe_priority(0, SACN_DMX_MERGER_SOURCE_INVALID, VALID_PRIORITY);

  etcpal_error_t valid_result = sacn_dmx_merger_update_universe_priority(0, 0, VALID_PRIORITY);

  EXPECT_EQ(invalid_merger_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_source_result, kEtcPalErrInvalid);

  EXPECT_NE(valid_result, kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, UpdateUniversePriorityErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_update_universe_priority(0, 0, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_update_universe_priority(0, 0, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, UpdateUniversePriorityErrNotFoundWorks)
{
  sacn_dmx_merger_source_t source = 0;

  etcpal_error_t no_merger_result = sacn_dmx_merger_update_universe_priority(merger_handle_, source, VALID_PRIORITY);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result = sacn_dmx_merger_update_universe_priority(merger_handle_, source, VALID_PRIORITY);

  sacn_dmx_merger_add_source(merger_handle_, &source);

  etcpal_error_t found_result = sacn_dmx_merger_update_universe_priority(merger_handle_, source, VALID_PRIORITY);

  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}

TEST_F(TestDmxMergerUpdate, StopSourcePapWorks)
{
  UpdateExpectedMergeResults(merge_source_1_, LOW_PRIORITY, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, HIGH_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, LOW_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                        test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, HIGH_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_paps(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                        test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_remove_paps(merger_handle_, merge_source_2_), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMerger, StopSourcePapErrNotFoundWorks)
{
  sacn_dmx_merger_source_t source = SACN_DMX_MERGER_SOURCE_INVALID;

  etcpal_error_t invalid_source_result = sacn_dmx_merger_remove_paps(merger_handle_, source);

  source = 1;

  etcpal_error_t no_merger_result = sacn_dmx_merger_remove_paps(merger_handle_, source);
  etcpal_error_t invalid_merger_result = sacn_dmx_merger_remove_paps(SACN_DMX_MERGER_INVALID, source);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result = sacn_dmx_merger_remove_paps(merger_handle_, source);

  sacn_dmx_merger_add_source(merger_handle_, &source);

  etcpal_error_t found_result = sacn_dmx_merger_remove_paps(merger_handle_, source);

  EXPECT_EQ(invalid_source_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(invalid_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}

TEST_F(TestDmxMerger, StopSourcePapErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_remove_paps(0, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_remove_paps(0, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, SourceIsValidWorks)
{
  sacn_dmx_merger_source_t slot_owners_array[DMX_ADDRESS_COUNT];
  memset(slot_owners_array, 1u, DMX_ADDRESS_COUNT * sizeof(sacn_dmx_merger_source_t));  // Fill with non-zero values.
  slot_owners_array[1] = SACN_DMX_MERGER_SOURCE_INVALID;  // Set one of them to invalid.

  EXPECT_EQ(SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners_array, 0), true);
  EXPECT_EQ(SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners_array, 1), false);
  EXPECT_EQ(SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners_array, 2), true);
}
