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
constexpr uint8_t LOW_PRIORITY = 10;
constexpr uint8_t MINIMUM_PRIORITY = 0;
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

    ASSERT_EQ(sacn_dmx_merger_init(), kEtcPalErrOk);

    merger_config_ = SACN_DMX_MERGER_CONFIG_INIT;
    merger_config_.levels = levels_;
    merger_config_.per_address_priorities = per_address_priorities_;
    merger_config_.per_address_priorities_active = &per_address_priorities_active_;
    merger_config_.universe_priority = &universe_priority_;
    merger_config_.owners = owners_;
    merger_config_.source_count_max = SACN_RECEIVER_INFINITE_SOURCES;
  }

  void TearDown() override { sacn_dmx_merger_deinit(); }

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

  uint8_t levels_[DMX_ADDRESS_COUNT];
  uint8_t per_address_priorities_[DMX_ADDRESS_COUNT];
  bool per_address_priorities_active_;
  uint8_t universe_priority_;
  sacn_dmx_merger_source_t owners_[DMX_ADDRESS_COUNT];
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
    if (sacn_dmx_merger_get_source(merger_handle_, merge_source_1_) != nullptr)
    {
      EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_1_), kEtcPalErrOk);
    }

    if (sacn_dmx_merger_get_source(merger_handle_, merge_source_2_) != nullptr)
    {
      EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
    }

    EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);

    TestDmxMerger::TearDown();
  }

  void UpdateExpectedMergeResults(sacn_dmx_merger_source_t source, std::optional<int> priority,
                                  std::optional<std::vector<uint8_t>> levels, std::optional<std::vector<uint8_t>> pap)
  {
    for (size_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      // If this slot's priority is being sourced:
      if (((!pap && priority) || (pap && (i < pap->size()) && (pap->at(i) != 0))))
      {
        // Determine the current level and priority for this slot.
        int current_level = (levels && (i < levels->size())) ? levels->at(i) : 0;
        int current_pap = (pap && (i < pap->size())) ? pap->at(i) : priority.value_or(0);

        if (!pap && priority && (current_pap == 0))
          current_pap = 1;  // Universe priority of 0 gets converted to PAP of 1

        // If this beats the current highest priority, update the expected values.
        if ((current_pap > expected_merge_priorities_[i]) ||
            ((current_pap == expected_merge_priorities_[i]) && (current_level > expected_merge_levels_[i])))
        {
          expected_merge_levels_[i] = (current_pap == 0) ? 0 : current_level;
          expected_merge_priorities_[i] = current_pap;
          expected_merge_winners_[i] = (current_pap == 0) ? SACN_DMX_MERGER_SOURCE_INVALID : source;
        }
      }
    }

    if (pap)
      expected_merge_pap_active_ = true;

    if (priority.value_or(0) > expected_merge_universe_priority_)
      expected_merge_universe_priority_ = priority.value_or(0);
  }

  bool VerifyMergeResults()
  {
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      uint8_t expected_level = static_cast<uint8_t>(expected_merge_levels_[i]);
      uint8_t expected_pap =
          (expected_merge_winners_[i] == SACN_DMX_MERGER_SOURCE_INVALID)
              ? 0u
              : ((expected_merge_priorities_[i] == 0) ? 1u : static_cast<uint8_t>(expected_merge_priorities_[i]));

      EXPECT_EQ(merger_config_.levels[i], expected_level) << "Test failed with i == " << i << ".";
      EXPECT_EQ(merger_config_.per_address_priorities[i], expected_pap) << "Test failed with i == " << i << ".";
      EXPECT_EQ(merger_config_.owners[i], expected_merge_winners_[i]) << "Test failed with i == " << i << ".";

      if ((merger_config_.levels[i] != expected_level) || (merger_config_.per_address_priorities[i] != expected_pap) ||
          (merger_config_.owners[i] != expected_merge_winners_[i]))
      {
        return false;  // Return early to avoid output spam.
      }
    }

    bool expected_merge_pap_active = expected_merge_pap_active_;
    uint8_t expected_merge_universe_priority = static_cast<uint8_t>(expected_merge_universe_priority_);
    EXPECT_EQ(*merger_config_.per_address_priorities_active, expected_merge_pap_active);
    EXPECT_EQ(*merger_config_.universe_priority, expected_merge_universe_priority);

    return (*merger_config_.per_address_priorities_active == expected_merge_pap_active) &&
           (*merger_config_.universe_priority == expected_merge_universe_priority);
  }

  void ClearExpectedMergeResults()
  {
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      expected_merge_levels_[i] = 0;
      expected_merge_priorities_[i] = 0;
      expected_merge_winners_[i] = SACN_DMX_MERGER_SOURCE_INVALID;
    }

    expected_merge_pap_active_ = false;
    expected_merge_universe_priority_ = 0;
  }

  sacn_dmx_merger_source_t merge_source_1_;
  sacn_dmx_merger_source_t merge_source_2_;
  int expected_merge_levels_[DMX_ADDRESS_COUNT];
  int expected_merge_priorities_[DMX_ADDRESS_COUNT];
  bool expected_merge_pap_active_;
  int expected_merge_universe_priority_;
  sacn_dmx_merger_source_t expected_merge_winners_[DMX_ADDRESS_COUNT];

  const std::vector<uint8_t> test_values_ascending_ = [&] {
    std::vector<uint8_t> vect(DMX_ADDRESS_COUNT);
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
      vect[i] = i % 256;
    return vect;
  }();

  const std::vector<uint8_t> test_values_partial_ascending_ = [&] {
    std::vector<uint8_t> vect(DMX_ADDRESS_COUNT);
    for (int i = 0; i < (DMX_ADDRESS_COUNT / 2); ++i)
      vect[i] = i % 256;
    return vect;
  }();

  const std::vector<uint8_t> test_values_descending_ = [&] {
    std::vector<uint8_t> vect(DMX_ADDRESS_COUNT);
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
      vect[i] = 255 - (i % 256);
    return vect;
  }();

  const std::vector<uint8_t> test_values_partial_descending_ = [&] {
    std::vector<uint8_t> vect(DMX_ADDRESS_COUNT);
    for (int i = 0; i < (DMX_ADDRESS_COUNT / 2); ++i)
      vect[i] = 255 - (i % 256);
    return vect;
  }();

  const std::vector<uint8_t> test_values_zero_ = [&] {
    std::vector<uint8_t> vect(DMX_ADDRESS_COUNT);
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
      vect[i] = 0;
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
  uint8_t expected_levels_priorities[DMX_ADDRESS_COUNT];
  sacn_dmx_merger_source_t expected_owners[DMX_ADDRESS_COUNT];

  for (uint16_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    levels_[i] = i % 0xff;
    owners_[i] = i;
    expected_levels_priorities[i] = 0;
    expected_owners[i] = SACN_DMX_MERGER_SOURCE_INVALID;
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
  EXPECT_EQ(memcmp(levels_, expected_levels_priorities, DMX_ADDRESS_COUNT), 0);
  EXPECT_EQ(memcmp(per_address_priorities_, expected_levels_priorities, DMX_ADDRESS_COUNT), 0);
  EXPECT_EQ(memcmp(owners_, expected_owners, sizeof(sacn_dmx_merger_source_t) * DMX_ADDRESS_COUNT), 0);

  // Make sure the correct merger state was created.
  EXPECT_EQ(get_number_of_mergers(), 1u);

  MergerState* merger_state = nullptr;
  lookup_state(merger_handle_, SACN_DMX_MERGER_SOURCE_INVALID, &merger_state, nullptr);
  ASSERT_NE(merger_state, nullptr);

  EXPECT_EQ(merger_state->handle, merger_handle_);
  EXPECT_EQ(merger_state->config.source_count_max, merger_config_.source_count_max);
  EXPECT_EQ(merger_state->config.levels, merger_config_.levels);
  EXPECT_EQ(merger_state->config.per_address_priorities, merger_config_.per_address_priorities);
  EXPECT_EQ(merger_state->config.owners, merger_config_.owners);
  EXPECT_EQ(memcmp(merger_state->config.per_address_priorities, expected_levels_priorities, DMX_ADDRESS_COUNT), 0);
  EXPECT_EQ(etcpal_rbtree_size(&merger_state->source_state_lookup), 0u);
}

TEST_F(TestDmxMerger, MergerCreateErrInvalidWorks)
{
  SacnDmxMergerConfig invalidLevelsConfig = merger_config_;
  invalidLevelsConfig.levels = NULL;

  SacnDmxMergerConfig invalidPapConfig = merger_config_;
  invalidPapConfig.per_address_priorities = NULL;

  SacnDmxMergerConfig invalidOwnersConfig = merger_config_;
  invalidOwnersConfig.owners = NULL;

  etcpal_error_t null_config_result = sacn_dmx_merger_create(NULL, &merger_handle_);
  etcpal_error_t null_handle_result = sacn_dmx_merger_create(&merger_config_, NULL);
  etcpal_error_t null_levels_result = sacn_dmx_merger_create(&invalidLevelsConfig, &merger_handle_);

  etcpal_error_t null_pap_result = sacn_dmx_merger_create(&invalidPapConfig, &merger_handle_);
  etcpal_error_t null_owners_result = sacn_dmx_merger_create(&invalidOwnersConfig, &merger_handle_);
  etcpal_error_t non_null_result = sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  EXPECT_EQ(null_config_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_handle_result, kEtcPalErrInvalid);
  EXPECT_EQ(null_levels_result, kEtcPalErrInvalid);

  EXPECT_NE(null_pap_result, kEtcPalErrInvalid);
  EXPECT_NE(null_owners_result, kEtcPalErrInvalid);
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
  EXPECT_EQ(source_state->source.using_universe_priority, true);

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
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, source_2_handle, priorities, DMX_ADDRESS_COUNT), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, source_2_handle, source_2_priority_2),
            kEtcPalErrOk);

  // Before removing a source, check the output.
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    if (i < (DMX_ADDRESS_COUNT / 2))
    {
      EXPECT_EQ(merger_config_.levels[i], source_1_value);
      EXPECT_EQ(merger_config_.per_address_priorities[i], source_1_priority);
      EXPECT_EQ(merger_config_.owners[i], source_1_handle);
    }
    else
    {
      EXPECT_EQ(merger_config_.levels[i], source_2_value);
      EXPECT_EQ(merger_config_.per_address_priorities[i], source_2_priority_2);
      EXPECT_EQ(merger_config_.owners[i], source_2_handle);
    }
  }

  EXPECT_TRUE(*merger_config_.per_address_priorities_active);
  EXPECT_EQ(*merger_config_.universe_priority, source_2_priority_2);

  // Now remove source 2 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_2_handle), kEtcPalErrOk);

  // The output should be just source 1 now.
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    EXPECT_EQ(merger_config_.levels[i], source_1_value);
    EXPECT_EQ(merger_config_.per_address_priorities[i], source_1_priority);
    EXPECT_EQ(merger_config_.owners[i], source_1_handle);
  }

  EXPECT_FALSE(*merger_config_.per_address_priorities_active);
  EXPECT_EQ(*merger_config_.universe_priority, source_1_priority);

  // Now remove source 1 and confirm success.
  EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_1_handle), kEtcPalErrOk);

  // The output should indicate that no levels are being sourced.
  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    EXPECT_EQ(merger_config_.levels[i], 0u);
    EXPECT_EQ(merger_config_.per_address_priorities[i], 0u);
    EXPECT_EQ(merger_config_.owners[i], SACN_DMX_MERGER_SOURCE_INVALID);
  }

  EXPECT_FALSE(*merger_config_.per_address_priorities_active);
  EXPECT_EQ(*merger_config_.universe_priority, 0u);
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

TEST_F(TestDmxMergerUpdate, MergesPapPermutation1)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, test_values_ascending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                       test_values_ascending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapPermutation2)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, test_values_ascending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                       test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapPermutation3)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, test_values_ascending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                       test_values_ascending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapPermutation4)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, test_values_ascending_);

  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_ascending_.data(),
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

TEST_F(TestDmxMergerUpdate, MergesUps4)
{
  UpdateExpectedMergeResults(merge_source_1_, MINIMUM_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, LOW_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, HIGH_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, LOW_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, MINIMUM_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapWithUps)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, VALID_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, VALID_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesUpsWithPap)
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
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_ascending_.data(),
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

  bool merge_results_pass = true;
  for (uint16_t i = 0; (i < test_values_descending_.size()) && merge_results_pass; ++i)
  {
    variable_levels.push_back(test_values_descending_[variable_levels.size()]);

    EXPECT_EQ(
        sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, variable_levels.data(), variable_levels.size()),
        kEtcPalErrOk);

    ClearExpectedMergeResults();
    UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);
    UpdateExpectedMergeResults(merge_source_2_, VALID_PRIORITY, variable_levels, std::nullopt);

    merge_results_pass = VerifyMergeResults();
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
}

TEST_F(TestDmxMergerUpdate, HandlesLessPap)
{
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);

  std::vector<uint8_t> variable_pap;

  bool merge_results_pass = true;
  for (uint16_t i = 0; (i < test_values_ascending_.size()) && merge_results_pass; ++i)
  {
    variable_pap.push_back(test_values_ascending_[variable_pap.size()]);

    EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, variable_pap.data(), variable_pap.size()),
              kEtcPalErrOk);

    ClearExpectedMergeResults();
    UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
    UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, variable_pap);

    merge_results_pass = VerifyMergeResults();
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
}

TEST_F(TestDmxMergerUpdate, MergesLevelsOnChange)
{
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, VALID_PRIORITY), kEtcPalErrOk);

  std::vector<uint8_t> variable_levels = test_values_descending_;

  bool merge_results_pass = true;
  for (uint16_t i = 1; (i < variable_levels.size()) && merge_results_pass; ++i)
  {
    if (variable_levels[i] < test_values_ascending_[i])
      variable_levels[i] = (test_values_ascending_[i] + 1) % 256;
    else
      variable_levels[i] = (test_values_ascending_[i] - 1) % 256;

    ClearExpectedMergeResults();
    UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);
    UpdateExpectedMergeResults(merge_source_2_, VALID_PRIORITY, variable_levels, std::nullopt);

    EXPECT_EQ(
        sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, variable_levels.data(), variable_levels.size()),
        kEtcPalErrOk);

    merge_results_pass = VerifyMergeResults();
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";

    // Again to test non-change detection
    if (merge_results_pass)
    {
      EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, variable_levels.data(),
                                              variable_levels.size()),
                kEtcPalErrOk);

      merge_results_pass = VerifyMergeResults();
      EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
    }
  }
}

TEST_F(TestDmxMergerUpdate, MergesPapOnChange)
{
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);

  std::vector<uint8_t> variable_pap = test_values_ascending_;

  bool merge_results_pass = true;
  for (uint16_t i = 0; (i < variable_pap.size()) && merge_results_pass; ++i)
  {
    if (variable_pap[i] < test_values_descending_[i])
      variable_pap[i] = (test_values_descending_[i] + 1) % 256;
    else
      variable_pap[i] = (test_values_descending_[i] - 1) % 256;

    ClearExpectedMergeResults();
    UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);
    UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_descending_, variable_pap);

    EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, variable_pap.data(), variable_pap.size()),
              kEtcPalErrOk);

    merge_results_pass = VerifyMergeResults();
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";

    // Again to test non-change detection
    if (merge_results_pass)
    {
      EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, variable_pap.data(), variable_pap.size()),
                kEtcPalErrOk);

      merge_results_pass = VerifyMergeResults();
      EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
    }
  }
}

TEST_F(TestDmxMergerUpdate, MergesUniversePriorityOnChange)
{
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);

  bool merge_results_pass = true;
  for (uint8_t variable_up = LOW_PRIORITY; (variable_up <= HIGH_PRIORITY) && merge_results_pass; ++variable_up)
  {
    ClearExpectedMergeResults();
    UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);
    UpdateExpectedMergeResults(merge_source_2_, variable_up, test_values_descending_, std::nullopt);

    EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, variable_up), kEtcPalErrOk);

    merge_results_pass = VerifyMergeResults();
    EXPECT_TRUE(merge_results_pass) << "Test failed with variable_up == " << variable_up << ".";

    // Again to test non-change detection
    if (merge_results_pass)
    {
      EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, variable_up), kEtcPalErrOk);
      VerifyMergeResults();
    }
  }
}

TEST_F(TestDmxMergerUpdate, MergesUnsourcedLevels1)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, std::nullopt, test_values_ascending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, std::nullopt, test_values_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                       test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesUnsourcedLevels2)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_ascending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, std::nullopt, test_values_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                       test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesUnsourcedLevels3)
{
  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, std::nullopt, test_values_ascending_);
  UpdateExpectedMergeResults(merge_source_2_, std::nullopt, test_values_ascending_, test_values_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                       test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_descending_.data(),
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
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_ascending_.data(),
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
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, ConvertsUp0ToPap1)
{
  UpdateExpectedMergeResults(merge_source_1_, MINIMUM_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, MINIMUM_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, MINIMUM_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, MINIMUM_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesLevelsAndUp)
{
  ASSERT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
  // Now only merge_source_1_ is left.

  UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_ascending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesPartialLevelsAndUp)
{
  ASSERT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
  // Now only merge_source_1_ is left.

  UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, test_values_partial_ascending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_partial_ascending_.data(),
                                          test_values_partial_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesLevelsAndPap)
{
  ASSERT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
  // Now only merge_source_1_ is left.

  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesPartialLevelsAndPap)
{
  ASSERT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
  // Now only merge_source_1_ is left.

  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_partial_ascending_, test_values_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_partial_ascending_.data(),
                                          test_values_partial_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesLevelsAndPartialPap)
{
  ASSERT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
  // Now only merge_source_1_ is left.

  UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_ascending_, test_values_partial_descending_);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_partial_descending_.data(),
                                       test_values_partial_descending_.size()),
            kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesLevelsAndUp0)
{
  ASSERT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
  // Now only merge_source_1_ is left.

  UpdateExpectedMergeResults(merge_source_1_, MINIMUM_PRIORITY, test_values_ascending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_ascending_.data(),
                                          test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, MINIMUM_PRIORITY), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, SingleSourceHandlesLevelCount)
{
  ASSERT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
  // Now only merge_source_1_ is left.

  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_1_, VALID_PRIORITY), kEtcPalErrOk);

  std::vector<uint8_t> variable_levels;

  bool merge_results_pass = true;
  for (uint16_t i = 0; (i < test_values_descending_.size()) && merge_results_pass; ++i)
  {
    variable_levels.push_back(test_values_descending_[variable_levels.size()]);

    EXPECT_EQ(
        sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, variable_levels.data(), variable_levels.size()),
        kEtcPalErrOk);

    ClearExpectedMergeResults();
    UpdateExpectedMergeResults(merge_source_1_, VALID_PRIORITY, variable_levels, std::nullopt);

    merge_results_pass = VerifyMergeResults();
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
}

TEST_F(TestDmxMergerUpdate, SingleSourceHandlesLessPap)
{
  ASSERT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
  // Now only merge_source_1_ is left.

  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);

  std::vector<uint8_t> variable_pap;

  bool merge_results_pass = true;
  for (uint16_t i = 0; (i < test_values_ascending_.size()) && merge_results_pass; ++i)
  {
    variable_pap.push_back(test_values_ascending_[variable_pap.size()]);

    EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, variable_pap.data(), variable_pap.size()),
              kEtcPalErrOk);

    ClearExpectedMergeResults();
    UpdateExpectedMergeResults(merge_source_1_, std::nullopt, test_values_descending_, variable_pap);

    merge_results_pass = VerifyMergeResults();
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
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

TEST_F(TestDmxMerger, UpdatePapErrInvalidWorks)
{
  uint8_t foo = 0;

  etcpal_error_t invalid_merger_result = sacn_dmx_merger_update_pap(SACN_DMX_MERGER_INVALID, 0, &foo, 1);
  etcpal_error_t invalid_source_result = sacn_dmx_merger_update_pap(0, SACN_DMX_MERGER_SOURCE_INVALID, &foo, 1);
  etcpal_error_t invalid_pap_result_1 = sacn_dmx_merger_update_pap(0, 0, nullptr, 0);
  etcpal_error_t invalid_pap_result_2 = sacn_dmx_merger_update_pap(0, 0, &foo, 0);
  etcpal_error_t invalid_pap_result_3 = sacn_dmx_merger_update_pap(0, 0, nullptr, 1);
  etcpal_error_t invalid_pap_result_4 = sacn_dmx_merger_update_pap(0, 0, &foo, DMX_ADDRESS_COUNT + 1);

  etcpal_error_t valid_result = sacn_dmx_merger_update_pap(0, 0, &foo, 1);

  EXPECT_EQ(invalid_merger_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_source_result, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_pap_result_1, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_pap_result_2, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_pap_result_3, kEtcPalErrInvalid);
  EXPECT_EQ(invalid_pap_result_4, kEtcPalErrInvalid);

  EXPECT_NE(valid_result, kEtcPalErrInvalid);
}

TEST_F(TestDmxMerger, UpdatePapErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_update_pap(0, 0, nullptr, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_update_pap(0, 0, nullptr, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, UpdatePapErrNotFoundWorks)
{
  uint8_t foo = 0;
  sacn_dmx_merger_source_t source = 0;

  etcpal_error_t no_merger_result = sacn_dmx_merger_update_pap(merger_handle_, source, &foo, 1);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result = sacn_dmx_merger_update_pap(merger_handle_, source, &foo, 1);

  sacn_dmx_merger_add_source(merger_handle_, &source);

  etcpal_error_t found_result = sacn_dmx_merger_update_pap(merger_handle_, source, &foo, 1);

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
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_1_, test_values_descending_.data(),
                                       test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, merge_source_2_, HIGH_PRIORITY), kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, merge_source_2_, test_values_descending_.data(),
                                          test_values_descending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, merge_source_2_, test_values_ascending_.data(),
                                       test_values_ascending_.size()),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_dmx_merger_remove_pap(merger_handle_, merge_source_2_), kEtcPalErrOk);

  VerifyMergeResults();

  ClearExpectedMergeResults();
  UpdateExpectedMergeResults(merge_source_1_, LOW_PRIORITY, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, HIGH_PRIORITY, test_values_descending_, std::nullopt);

  EXPECT_EQ(sacn_dmx_merger_remove_pap(merger_handle_, merge_source_1_), kEtcPalErrOk);

  VerifyMergeResults();
}

TEST_F(TestDmxMerger, StopSourcePapErrNotFoundWorks)
{
  sacn_dmx_merger_source_t source = SACN_DMX_MERGER_SOURCE_INVALID;

  etcpal_error_t invalid_source_result = sacn_dmx_merger_remove_pap(merger_handle_, source);

  source = 1;

  etcpal_error_t no_merger_result = sacn_dmx_merger_remove_pap(merger_handle_, source);
  etcpal_error_t invalid_merger_result = sacn_dmx_merger_remove_pap(SACN_DMX_MERGER_INVALID, source);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result = sacn_dmx_merger_remove_pap(merger_handle_, source);

  sacn_dmx_merger_add_source(merger_handle_, &source);

  etcpal_error_t found_result = sacn_dmx_merger_remove_pap(merger_handle_, source);

  EXPECT_EQ(invalid_source_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(invalid_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}

TEST_F(TestDmxMerger, StopSourcePapErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;
  etcpal_error_t not_initialized_result = sacn_dmx_merger_remove_pap(0, 0);

  sacn_initialized_fake.return_val = true;
  etcpal_error_t initialized_result = sacn_dmx_merger_remove_pap(0, 0);

  EXPECT_EQ(not_initialized_result, kEtcPalErrNotInit);
  EXPECT_NE(initialized_result, kEtcPalErrNotInit);
}

TEST_F(TestDmxMerger, SourceIsValidWorks)
{
  sacn_dmx_merger_source_t owners_array[DMX_ADDRESS_COUNT];
  memset(owners_array, 1u, DMX_ADDRESS_COUNT * sizeof(sacn_dmx_merger_source_t));  // Fill with non-zero values.
  owners_array[1] = SACN_DMX_MERGER_SOURCE_INVALID;                                // Set one of them to invalid.

  EXPECT_EQ(SACN_DMX_MERGER_SOURCE_IS_VALID(owners_array, 0), true);
  EXPECT_EQ(SACN_DMX_MERGER_SOURCE_IS_VALID(owners_array, 1), false);
  EXPECT_EQ(SACN_DMX_MERGER_SOURCE_IS_VALID(owners_array, 2), true);
}
