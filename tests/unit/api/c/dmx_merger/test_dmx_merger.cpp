/******************************************************************************
 * Copyright 2023 ETC Inc.
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

#include <algorithm>
#include <functional>
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

constexpr uint8_t kValidPriority = 100;
constexpr uint8_t kLowPriority = 10;
constexpr uint8_t kMinimumPriority = 0;
constexpr uint8_t kHighPriority = 200;

class TestDmxMerger : public ::testing::Test
{
public:
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

  void UpdateLevels(sacn_dmx_merger_source_t& source, const std::vector<uint8_t>& new_levels)
  {
    if (sacn_dmx_merger_get_source(merger_handle_, source) == nullptr)
    {
      ASSERT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source), kEtcPalErrOk);
    }

    EXPECT_EQ(sacn_dmx_merger_update_levels(merger_handle_, source, new_levels.data(), new_levels.size()),
              kEtcPalErrOk);
  }

  void UpdatePap(sacn_dmx_merger_source_t& source, const std::vector<uint8_t>& pap)
  {
    if (sacn_dmx_merger_get_source(merger_handle_, source) == nullptr)
    {
      ASSERT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source), kEtcPalErrOk);
    }

    EXPECT_EQ(sacn_dmx_merger_update_pap(merger_handle_, source, pap.data(), pap.size()), kEtcPalErrOk);
  }

  void UpdateUniversePriority(sacn_dmx_merger_source_t& source, int universe_priority)
  {
    ASSERT_LE(universe_priority, 0xFF);

    if (sacn_dmx_merger_get_source(merger_handle_, source) == nullptr)
    {
      ASSERT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source), kEtcPalErrOk);
    }

    EXPECT_EQ(sacn_dmx_merger_update_universe_priority(merger_handle_, source, static_cast<uint8_t>(universe_priority)),
              kEtcPalErrOk);
  }

protected:
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
public:
  class MergerCall
  {
  public:
    MergerCall(const std::function<void()>& call) : id_(next_id_++), call_(call) {}

    void operator()() const { call_(); }
    bool operator<(const MergerCall& rhs) const { return id_ < rhs.id_; }
    bool operator==(const MergerCall& rhs) const { return id_ == rhs.id_; }

  private:
    static int next_id_;

    int id_;
    std::function<void()> call_;
  };

  enum class TestMergeMode
  {
    kRefreshEachPermutation,
    kNoRefreshing
  };

  struct TestPermutationsInfo
  {
    TestMergeMode merge_mode{TestMergeMode::kRefreshEachPermutation};
    std::function<void()> setup_expectations_fn{[]() {}};
    std::vector<MergerCall> merger_calls{};
  };

  struct TestMergeInfo
  {
    TestMergeMode merge_mode{TestMergeMode::kRefreshEachPermutation};

    std::optional<std::vector<uint8_t>> src_1_levels;
    std::optional<std::vector<uint8_t>> src_1_paps;
    std::optional<int> src_1_universe_priority;

    std::optional<std::vector<uint8_t>> src_2_levels;
    std::optional<std::vector<uint8_t>> src_2_paps;
    std::optional<int> src_2_universe_priority;
  };

  static constexpr size_t kNumTestIterations = 5u;

  void SetUp() override
  {
    TestDmxMerger::SetUp();

    // Initialize the merger.
    EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

    // Initialize expected merge results.
    ClearExpectedMergeResults();
  }

  void TearDown() override
  {
    // Deinitialize the sources and merger.
    RemoveAllSources();

    EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);

    TestDmxMerger::TearDown();
  }

  void UpdateExpectedMergeResults(sacn_dmx_merger_source_t source, std::optional<int> priority,
                                  std::optional<std::vector<uint8_t>> levels, std::optional<std::vector<uint8_t>> pap)
  {
    for (size_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
    {
      bool level_sourced = levels && (i < levels->size());
      bool priority_sourced = (!pap && priority) || (pap && (i < pap->size()) && (pap->at(i) != 0));

      // If this slot is being sourced:
      if (level_sourced && priority_sourced)
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

  bool VerifyAllPermutations(const TestPermutationsInfo& info)
  {
    std::vector<MergerCall> current_permutation = info.merger_calls;
    std::sort(current_permutation.begin(), current_permutation.end());

    int permutation_num = 1;
    bool time_to_refresh = true;
    do
    {
      if (time_to_refresh)
        RefreshMerger();

      std::string current_permutation_readable;
      for (const auto& merger_call : current_permutation)
      {
        merger_call();

        // Generate string to identify the current permutation (vector indexes) on failure.
        for (size_t i = 0u; i < info.merger_calls.size(); ++i)
        {
          if (merger_call == info.merger_calls[i])
          {
            current_permutation_readable.append(" ");
            current_permutation_readable.append(std::to_string(i + 1));
          }
        }
      }

      if (time_to_refresh)
      {
        ClearExpectedMergeResults();
        info.setup_expectations_fn();
      }

      bool merge_results_pass = VerifyMergeResults();
      EXPECT_TRUE(merge_results_pass) << "Test failed for permutation " << permutation_num
                                      << " (1-based merger_calls order:" << current_permutation_readable << ").";

      if (!merge_results_pass)
        return false;  // Return early to avoid spam.

      if (info.merge_mode == TestMergeMode::kNoRefreshing)
        time_to_refresh = false;  // Only refresh the first time for initialization.

      ++permutation_num;
    } while (std::next_permutation(current_permutation.begin(), current_permutation.end()));

    return true;
  }

  bool VerifyMerge(const TestMergeInfo& info)
  {
    return VerifyAllPermutations(
        {.merge_mode = info.merge_mode,
         .setup_expectations_fn =
             [&]() {
               UpdateExpectedMergeResults(merge_source_1_, info.src_1_universe_priority, info.src_1_levels,
                                          info.src_1_paps);
               UpdateExpectedMergeResults(merge_source_2_, info.src_2_universe_priority, info.src_2_levels,
                                          info.src_2_paps);
             },
         .merger_calls = {MergerCall([&]() {  // Element 1 (source 1 PAP)
                            if (info.src_1_paps)
                              UpdatePap(merge_source_1_, *info.src_1_paps);
                          }),
                          MergerCall([&]() {  // Element 2 (source 2 PAP)
                            if (info.src_2_paps)
                              UpdatePap(merge_source_2_, *info.src_2_paps);
                          }),
                          MergerCall([&]() {  // Element 3 (source 1 levels)
                            if (info.src_1_levels)
                              UpdateLevels(merge_source_1_, *info.src_1_levels);
                          }),
                          MergerCall([&]() {  // Element 4 (source 2 levels)
                            if (info.src_2_levels)
                              UpdateLevels(merge_source_2_, *info.src_2_levels);
                          }),
                          MergerCall([&]() {  // Element 5 (source 1 universe priority)
                            if (info.src_1_universe_priority)
                              UpdateUniversePriority(merge_source_1_, *info.src_1_universe_priority);
                          }),
                          MergerCall([&]() {  // Element 6 (source 2 universe priority)
                            if (info.src_2_universe_priority)
                              UpdateUniversePriority(merge_source_2_, *info.src_2_universe_priority);
                          })}});
  }

  void RemoveAllSources()
  {
    if (sacn_dmx_merger_get_source(merger_handle_, merge_source_1_) != nullptr)
    {
      EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_1_), kEtcPalErrOk);
      merge_source_1_ = SACN_DMX_MERGER_SOURCE_INVALID;
    }

    if (sacn_dmx_merger_get_source(merger_handle_, merge_source_2_) != nullptr)
    {
      EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, merge_source_2_), kEtcPalErrOk);
      merge_source_2_ = SACN_DMX_MERGER_SOURCE_INVALID;
    }
  }

  void RefreshMerger()
  {
    RemoveAllSources();
    EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);
  }

protected:
  sacn_dmx_merger_source_t merge_source_1_{SACN_DMX_MERGER_SOURCE_INVALID};
  sacn_dmx_merger_source_t merge_source_2_{SACN_DMX_MERGER_SOURCE_INVALID};
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

int TestDmxMergerUpdate::MergerCall::next_id_ = 0;

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

#if SACN_DMX_MERGER_DISABLE_INTERNAL_PAP_BUFFER
  EXPECT_EQ(null_pap_result, kEtcPalErrInvalid);
#else
  EXPECT_NE(null_pap_result, kEtcPalErrInvalid);
#endif

#if SACN_DMX_MERGER_DISABLE_INTERNAL_OWNER_BUFFER
  EXPECT_EQ(null_owners_result, kEtcPalErrInvalid);
#else
  EXPECT_NE(null_owners_result, kEtcPalErrInvalid);
#endif

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
  auto source_1_values = std::vector<uint8_t>(DMX_ADDRESS_COUNT, source_1_value);

  UpdateLevels(source_1_handle, source_1_values);
  UpdateUniversePriority(source_1_handle, priority);

  // Feed in data from source 2 with per-address-priorities, one half lower and one half higher.
  auto source_2_values = std::vector<uint8_t>(DMX_ADDRESS_COUNT, source_2_value);
  auto source_2_priorities = std::vector<uint8_t>(DMX_ADDRESS_COUNT / 2, source_2_priority_1);
  auto source_2_priorities_pt2 = std::vector<uint8_t>(DMX_ADDRESS_COUNT / 2, source_2_priority_2);
  source_2_priorities.insert(source_2_priorities.end(), source_2_priorities_pt2.begin(), source_2_priorities_pt2.end());

  UpdateLevels(source_2_handle, source_2_values);
  UpdatePap(source_2_handle, source_2_priorities);
  UpdateUniversePriority(source_2_handle, source_2_priority_2);

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

TEST_F(TestDmxMergerUpdate, MergesLevels)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_universe_priority = kValidPriority,
               .src_2_levels = test_values_descending_,
               .src_2_universe_priority = kValidPriority});
}

TEST_F(TestDmxMergerUpdate, MergesPaps)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_paps = test_values_descending_,
               .src_2_levels = test_values_descending_,
               .src_2_paps = test_values_ascending_});
}

TEST_F(TestDmxMergerUpdate, MergesUps1)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_universe_priority = kLowPriority,
               .src_2_levels = test_values_descending_,
               .src_2_universe_priority = kLowPriority});
}

TEST_F(TestDmxMergerUpdate, MergesUps2)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_universe_priority = kLowPriority,
               .src_2_levels = test_values_descending_,
               .src_2_universe_priority = kHighPriority});
}

TEST_F(TestDmxMergerUpdate, MergesUps3)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_universe_priority = kHighPriority,
               .src_2_levels = test_values_descending_,
               .src_2_universe_priority = kLowPriority});
}

TEST_F(TestDmxMergerUpdate, MergesUps4)
{
  UpdateLevels(merge_source_1_, test_values_ascending_);
  UpdateUniversePriority(merge_source_1_, kHighPriority);
  UpdateLevels(merge_source_2_, test_values_descending_);
  UpdateUniversePriority(merge_source_2_, kLowPriority);
  UpdateUniversePriority(merge_source_1_, kMinimumPriority);

  UpdateExpectedMergeResults(merge_source_1_, kMinimumPriority, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, kLowPriority, test_values_descending_, std::nullopt);
  VerifyMergeResults();
}

TEST_F(TestDmxMergerUpdate, MergesPapWithUps)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_paps = test_values_descending_,
               .src_2_levels = test_values_descending_,
               .src_2_universe_priority = kValidPriority});
}

TEST_F(TestDmxMergerUpdate, MergesUpsWithPap)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_universe_priority = kValidPriority,
               .src_2_levels = test_values_descending_,
               .src_2_paps = test_values_ascending_});
}

TEST_F(TestDmxMergerUpdate, HandlesLevelCount)
{
  ASSERT_LE(kNumTestIterations, test_values_descending_.size());

  std::vector<uint8_t> variable_levels;

  bool merge_results_pass = true;
  for (uint16_t i = 0; (i < kNumTestIterations) && merge_results_pass; ++i)
  {
    variable_levels.push_back(test_values_descending_[variable_levels.size()]);

    merge_results_pass = VerifyMerge({.merge_mode = TestMergeMode::kNoRefreshing,
                                      .src_1_levels = test_values_ascending_,
                                      .src_1_universe_priority = kValidPriority,
                                      .src_2_levels = variable_levels,
                                      .src_2_universe_priority = kValidPriority});
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
}

TEST_F(TestDmxMergerUpdate, HandlesLessPap)
{
  ASSERT_LE(kNumTestIterations, test_values_ascending_.size());

  std::vector<uint8_t> variable_pap;

  bool merge_results_pass = true;
  for (uint16_t i = 0; (i < kNumTestIterations) && merge_results_pass; ++i)
  {
    variable_pap.push_back(test_values_ascending_[variable_pap.size()]);

    merge_results_pass = VerifyMerge({.merge_mode = TestMergeMode::kNoRefreshing,
                                      .src_1_levels = test_values_ascending_,
                                      .src_1_paps = test_values_descending_,
                                      .src_2_levels = test_values_descending_,
                                      .src_2_paps = variable_pap});
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
}

TEST_F(TestDmxMergerUpdate, MergesLevelsOnChange)
{
  std::vector<uint8_t> variable_levels = test_values_descending_;

  size_t start_offset = 256 - (kNumTestIterations / 2);
  ASSERT_LE(start_offset + kNumTestIterations, variable_levels.size());

  bool merge_results_pass = true;
  for (size_t i = start_offset; (i < (start_offset + kNumTestIterations)) && merge_results_pass; ++i)
  {
    if (variable_levels[i] < test_values_ascending_[i])
      variable_levels[i] = (test_values_ascending_[i] + 1) % 256;
    else
      variable_levels[i] = (test_values_ascending_[i] - 1) % 256;

    merge_results_pass = VerifyMerge({.merge_mode = TestMergeMode::kNoRefreshing,
                                      .src_1_levels = test_values_ascending_,
                                      .src_1_universe_priority = kValidPriority,
                                      .src_2_levels = variable_levels,
                                      .src_2_universe_priority = kValidPriority});
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
}

TEST_F(TestDmxMergerUpdate, MergesPapOnChange)
{
  std::vector<uint8_t> variable_pap = test_values_ascending_;

  size_t start_offset = 256 - (kNumTestIterations / 2);
  ASSERT_LE(start_offset + kNumTestIterations, variable_pap.size());

  bool merge_results_pass = true;
  for (size_t i = start_offset; (i < (start_offset + kNumTestIterations)) && merge_results_pass; ++i)
  {
    if (variable_pap[i] < test_values_descending_[i])
      variable_pap[i] = (test_values_descending_[i] + 1) % 256;
    else
      variable_pap[i] = (test_values_descending_[i] - 1) % 256;

    merge_results_pass = VerifyMerge({.merge_mode = TestMergeMode::kNoRefreshing,
                                      .src_1_levels = test_values_ascending_,
                                      .src_1_paps = test_values_descending_,
                                      .src_2_levels = test_values_descending_,
                                      .src_2_paps = variable_pap});
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
}

TEST_F(TestDmxMergerUpdate, MergesUniversePriorityOnChange)
{
  ASSERT_LT(kNumTestIterations / 2, kValidPriority);
  int start_priority = kValidPriority - (kNumTestIterations / 2);

  bool merge_results_pass = true;
  for (int variable_up = start_priority; (variable_up <= (start_priority + kNumTestIterations)) && merge_results_pass;
       ++variable_up)
  {
    merge_results_pass = VerifyMerge({.merge_mode = TestMergeMode::kNoRefreshing,
                                      .src_1_levels = test_values_ascending_,
                                      .src_1_universe_priority = kValidPriority,
                                      .src_2_levels = test_values_descending_,
                                      .src_2_universe_priority = variable_up});
    EXPECT_TRUE(merge_results_pass) << "Test failed with variable_up == " << variable_up << ".";
  }
}

TEST_F(TestDmxMergerUpdate, HandlesUnsourcedLevels1)
{
  VerifyMerge({.src_1_paps = test_values_ascending_, .src_2_paps = test_values_descending_});
}

TEST_F(TestDmxMergerUpdate, HandlesUnsourcedLevels2)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_paps = test_values_ascending_,
               .src_2_paps = test_values_descending_});
}

TEST_F(TestDmxMergerUpdate, HandlesUnsourcedLevels3)
{
  VerifyMerge({.src_1_paps = test_values_ascending_,
               .src_2_levels = test_values_ascending_,
               .src_2_paps = test_values_descending_});
}

TEST_F(TestDmxMergerUpdate, DoesNotMergeWithoutUpOrPap1)
{
  VerifyMerge({.src_1_levels = test_values_ascending_, .src_2_levels = test_values_descending_});
}

TEST_F(TestDmxMergerUpdate, DoesNotMergeWithoutUpOrPap2)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_paps = test_values_ascending_,
               .src_2_levels = test_values_descending_});
}

TEST_F(TestDmxMergerUpdate, DoesNotMergeWithoutUpOrPap3)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_2_levels = test_values_ascending_,
               .src_2_paps = test_values_descending_});
}

TEST_F(TestDmxMergerUpdate, ConvertsUp0ToPap1)
{
  VerifyMerge({.src_1_levels = test_values_ascending_,
               .src_1_universe_priority = kMinimumPriority,
               .src_2_levels = test_values_descending_,
               .src_2_universe_priority = kMinimumPriority});
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesLevelsAndUp)
{
  VerifyMerge({.src_1_levels = test_values_ascending_, .src_1_universe_priority = kValidPriority});
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesPartialLevelsAndUp)
{
  VerifyMerge({.src_1_levels = test_values_partial_ascending_, .src_1_universe_priority = kValidPriority});
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesLevelsAndPap)
{
  VerifyMerge({.src_1_levels = test_values_ascending_, .src_1_paps = test_values_descending_});
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesPartialLevelsAndPap)
{
  VerifyMerge({.src_1_levels = test_values_partial_ascending_, .src_1_paps = test_values_descending_});
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesLevelsAndPartialPap)
{
  VerifyMerge({.src_1_levels = test_values_ascending_, .src_1_paps = test_values_partial_descending_});
}

TEST_F(TestDmxMergerUpdate, SingleSourceMergesLevelsAndUp0)
{
  VerifyMerge({.src_1_levels = test_values_ascending_, .src_1_universe_priority = kMinimumPriority});
}

TEST_F(TestDmxMergerUpdate, SingleSourceHandlesLevelCount)
{
  ASSERT_LE(kNumTestIterations, test_values_descending_.size());

  std::vector<uint8_t> variable_levels;

  bool merge_results_pass = true;
  for (uint16_t i = 0; (i < kNumTestIterations) && merge_results_pass; ++i)
  {
    variable_levels.push_back(test_values_descending_[variable_levels.size()]);

    merge_results_pass = VerifyMerge({.merge_mode = TestMergeMode::kNoRefreshing,
                                      .src_1_levels = variable_levels,
                                      .src_1_universe_priority = kValidPriority});
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
}

TEST_F(TestDmxMergerUpdate, SingleSourceHandlesLessPap)
{
  ASSERT_LE(kNumTestIterations, test_values_ascending_.size());

  std::vector<uint8_t> variable_pap;

  bool merge_results_pass = true;
  for (uint16_t i = 0; (i < kNumTestIterations) && merge_results_pass; ++i)
  {
    variable_pap.push_back(test_values_ascending_[variable_pap.size()]);

    merge_results_pass = VerifyMerge({.merge_mode = TestMergeMode::kNoRefreshing,
                                      .src_1_levels = test_values_descending_,
                                      .src_1_paps = variable_pap});
    EXPECT_TRUE(merge_results_pass) << "Test failed with i == " << i << ".";
  }
}

TEST_F(TestDmxMergerUpdate, LevelsNotWrittenForReleasedSlots)
{
  VerifyMerge({.src_1_levels = {{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}},
               .src_1_paps = {{100, 100, 100, 100, 100, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
               .src_1_universe_priority = kValidPriority,
               .src_2_levels = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
               .src_2_paps = {{100, 100, 100, 150, 150, 150, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
               .src_2_universe_priority = kValidPriority});
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
      sacn_dmx_merger_update_universe_priority(SACN_DMX_MERGER_INVALID, 0, kValidPriority);
  etcpal_error_t invalid_source_result =
      sacn_dmx_merger_update_universe_priority(0, SACN_DMX_MERGER_SOURCE_INVALID, kValidPriority);

  etcpal_error_t valid_result = sacn_dmx_merger_update_universe_priority(0, 0, kValidPriority);

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

  etcpal_error_t no_merger_result = sacn_dmx_merger_update_universe_priority(merger_handle_, source, kValidPriority);

  sacn_dmx_merger_create(&merger_config_, &merger_handle_);

  etcpal_error_t no_source_result = sacn_dmx_merger_update_universe_priority(merger_handle_, source, kValidPriority);

  sacn_dmx_merger_add_source(merger_handle_, &source);

  etcpal_error_t found_result = sacn_dmx_merger_update_universe_priority(merger_handle_, source, kValidPriority);

  EXPECT_EQ(no_merger_result, kEtcPalErrNotFound);
  EXPECT_EQ(no_source_result, kEtcPalErrNotFound);

  EXPECT_NE(found_result, kEtcPalErrNotFound);
}

TEST_F(TestDmxMergerUpdate, StopSourcePapWorks)
{
  UpdateUniversePriority(merge_source_1_, kLowPriority);
  UpdateLevels(merge_source_1_, test_values_ascending_);
  UpdatePap(merge_source_1_, test_values_descending_);
  UpdateUniversePriority(merge_source_2_, kHighPriority);
  UpdateLevels(merge_source_2_, test_values_descending_);
  UpdatePap(merge_source_2_, test_values_ascending_);
  EXPECT_EQ(sacn_dmx_merger_remove_pap(merger_handle_, merge_source_2_), kEtcPalErrOk);

  UpdateExpectedMergeResults(merge_source_1_, kLowPriority, test_values_ascending_, test_values_descending_);
  UpdateExpectedMergeResults(merge_source_2_, kHighPriority, test_values_descending_, std::nullopt);
  VerifyMergeResults();

  EXPECT_EQ(sacn_dmx_merger_remove_pap(merger_handle_, merge_source_1_), kEtcPalErrOk);

  ClearExpectedMergeResults();
  UpdateExpectedMergeResults(merge_source_1_, kLowPriority, test_values_ascending_, std::nullopt);
  UpdateExpectedMergeResults(merge_source_2_, kHighPriority, test_values_descending_, std::nullopt);
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

TEST_F(TestDmxMerger, RespectsMaxLimits)
{
  for (int i = 0; i < SACN_DMX_MERGER_MAX_MERGERS; ++i)
  {
    EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

    sacn_dmx_merger_source_t source_handle;
    for (int j = 0; j < SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER; ++j)
      EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle), kEtcPalErrOk);
  }
}

TEST_F(TestDmxMerger, HandlesManySourcesAppearing)
{
  static constexpr int kNumIterations = 0x10000;  // Cause 16-bit source handles to wrap around

  EXPECT_EQ(sacn_dmx_merger_create(&merger_config_, &merger_handle_), kEtcPalErrOk);

  for (int i = 0; i < kNumIterations; ++i)
  {
    sacn_dmx_merger_source_t source_handle;
    EXPECT_EQ(sacn_dmx_merger_add_source(merger_handle_, &source_handle), kEtcPalErrOk);
    EXPECT_EQ(sacn_dmx_merger_remove_source(merger_handle_, source_handle), kEtcPalErrOk);
  }

  EXPECT_EQ(sacn_dmx_merger_destroy(merger_handle_), kEtcPalErrOk);
}
