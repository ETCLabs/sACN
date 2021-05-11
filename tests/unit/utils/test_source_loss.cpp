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

#include "sacn/private/source_loss.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <vector>
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/timer.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn_mock/private/common.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestSourceLoss TestSourceLossDynamic
#else
#define TestSourceLoss TestSourceLossStatic
#endif

bool operator<(const SacnRemoteSourceInternal& a, const SacnRemoteSourceInternal& b)
{
  return a.handle < b.handle;
}

bool operator==(const SacnRemoteSourceInternal& a, const SacnRemoteSourceInternal& b)
{
  return (a.handle == b.handle && (0 == std::strcmp(a.name, b.name)));
}

class TestSourceLoss : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_source_loss_init(), kEtcPalErrOk);

    expired_sources_ = get_sources_lost_buffer(0, SACN_RECEIVER_MAX_UNIVERSES);
    ASSERT_NE(expired_sources_, nullptr);

    sources_.reserve(SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
    test_names_.reserve(SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);

    for (size_t i = 0; i < SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE; ++i)
    {
      sacn_remote_source_t next_handle = SACN_REMOTE_SOURCE_INVALID;
      ASSERT_EQ(add_remote_source_handle(&etcpal::Uuid::V4().get(), &next_handle), kEtcPalErrOk);

      test_names_.push_back("test name " + std::to_string(i));
      sources_.push_back(SacnRemoteSourceInternal{next_handle, test_names_[i].c_str()});
    }
    std::sort(sources_.begin(), sources_.end());
  }

  void TearDown() override
  {
    sacn_source_loss_deinit();
    sacn_mem_deinit();
  }

  void VerifySourcesMatch(const SacnLostSource* lost_sources, size_t num_lost_sources,
                          const std::vector<SacnRemoteSourceInternal>& sources)
  {
    std::vector<SacnRemoteSourceInternal> expired_sources;
    expired_sources.reserve(num_lost_sources);
    std::transform(lost_sources, lost_sources + num_lost_sources, std::back_inserter(expired_sources),
                   [](const SacnLostSource& src) {
                     return SacnRemoteSourceInternal{src.handle, src.name};
                   });

    std::sort(expired_sources.begin(), expired_sources.end());

    EXPECT_EQ(expired_sources, sources);
  }

  std::vector<std::string> test_names_;
  std::vector<SacnRemoteSourceInternal> sources_;
  TerminationSet* term_set_list_{nullptr};
  SourcesLostNotification* expired_sources_{nullptr};
};

// Test the case where all sources are marked offline in the same tick. In this case, we should get
// all sources lost in the same notification.
TEST_F(TestSourceLoss, AllSourcesOfflineAtOnce)
{
  std::vector<SacnLostSourceInternal> offline_sources;
  offline_sources.reserve(SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  std::transform(sources_.begin(), sources_.end(), std::back_inserter(offline_sources),
                 [](const SacnRemoteSourceInternal& source) {
                   return SacnLostSourceInternal{source.handle, source.name, true};
                 });
  mark_sources_offline(offline_sources.data(), offline_sources.size(), sources_.data(), sources_.size(),
                       &term_set_list_, 1000);

  // The expired notification wait time has not passed yet, so we should not get a notification
  // yet.
  get_expired_sources(&term_set_list_, expired_sources_);
  EXPECT_EQ(expired_sources_->num_lost_sources, 0u);

  // Advance time by 1.1 seconds
  etcpal_getms_fake.return_val = 1100;

  // We should now get our notification containing all sources
  get_expired_sources(&term_set_list_, expired_sources_);
  EXPECT_EQ(expired_sources_->num_lost_sources, static_cast<size_t>(SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE));

  // Check to make sure the list of sources lost matches our original source list.
  VerifySourcesMatch(expired_sources_->lost_sources, expired_sources_->num_lost_sources, sources_);

  // And the termination set should be cleaned up
  EXPECT_EQ(term_set_list_, nullptr);
}

// Simulate each source expiring on a different call to tick, without any of them being marked back
// online in the interim. In this case, we should get all sources lost in the same notification.
TEST_F(TestSourceLoss, AllSourcesOfflineOneByOne)
{
  for (size_t i = 0; i < sources_.size() - 1; ++i)
  {
    SacnLostSourceInternal offline;
    offline.handle = sources_[i].handle;
    offline.name = sources_[i].name;
    offline.terminated = false;
    mark_sources_offline(&offline, 1, &sources_[i + 1], sources_.size() - i - 1, &term_set_list_, 1000);
    get_expired_sources(&term_set_list_, expired_sources_);
    EXPECT_EQ(expired_sources_->num_lost_sources, 0u);
    // Advance time by 50ms
    etcpal_getms_fake.return_val += 50;
  }

  SacnLostSourceInternal offline;
  offline.handle = sources_[sources_.size() - 1].handle;
  offline.name = sources_[sources_.size() - 1].name;
  offline.terminated = false;
  mark_sources_offline(&offline, 1, nullptr, 0, &term_set_list_, 1000);

  // The expired notification wait time has not passed yet, so we should not get a notification
  // yet.
  get_expired_sources(&term_set_list_, expired_sources_);
  EXPECT_EQ(expired_sources_->num_lost_sources, 0u);

  // Advance time by 1.1 seconds
  etcpal_getms_fake.return_val = 1100;

  // We should now get our one notification containing all sources
  get_expired_sources(&term_set_list_, expired_sources_);
  ASSERT_EQ(expired_sources_->num_lost_sources, static_cast<size_t>(SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE));

  // Check to make sure the list of sources lost matches our original source list.
  VerifySourcesMatch(expired_sources_->lost_sources, expired_sources_->num_lost_sources, sources_);

  // And the termination set should be cleaned up.
  EXPECT_EQ(term_set_list_, nullptr);
}

// Simulate each source expiring on a different call to tick, with all remaining sources remaining
// online in between. In this case, we should get each source lost in a different notification.
TEST_F(TestSourceLoss, WorstCaseEachSourceOfflineIndividually)
{
  std::vector<SacnLostSourceInternal> offline;
  offline.reserve(SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);

  for (size_t i = 0; i < sources_.size(); ++i)
  {
    offline.push_back(SacnLostSourceInternal{sources_[i].handle, sources_[i].name, false});
    if (i < sources_.size() - 1)
    {
      mark_sources_offline(offline.data(), offline.size(), &sources_[i + 1], sources_.size() - i - 1, &term_set_list_,
                           1000);
      mark_sources_online(&sources_[i + 1], sources_.size() - 1 - i, term_set_list_);
    }
    else
    {
      mark_sources_offline(offline.data(), offline.size(), nullptr, 0, &term_set_list_, 1000);
    }
    get_expired_sources(&term_set_list_, expired_sources_);
    EXPECT_EQ(expired_sources_->num_lost_sources, 0u);
    // Advance time by 50ms
    etcpal_getms_fake.return_val += 50;
  }

  // None of the timeouts have expired yet.
  mark_sources_offline(offline.data(), offline.size(), nullptr, 0, &term_set_list_, 1000);
  get_expired_sources(&term_set_list_, expired_sources_);
  EXPECT_EQ(expired_sources_->num_lost_sources, 0u);

  etcpal_getms_fake.return_val = 1001;

  // Now we should get one expired notification every 50 ms
  std::vector<SacnLostSource> lost_sources;
  lost_sources.reserve(SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  for (size_t i = 0; i < sources_.size(); ++i)
  {
    mark_sources_offline(&offline[i], offline.size() - i, nullptr, 0, &term_set_list_, 1000);
    expired_sources_ = get_sources_lost_buffer(0, SACN_RECEIVER_MAX_UNIVERSES);
    get_expired_sources(&term_set_list_, expired_sources_);
    ASSERT_EQ(expired_sources_->num_lost_sources, 1u) << "Failed on iteration" << i;
    lost_sources.push_back(expired_sources_->lost_sources[0]);
    etcpal_getms_fake.return_val += 50;
  }

  VerifySourcesMatch(lost_sources.data(), lost_sources.size(), sources_);
  EXPECT_EQ(term_set_list_, nullptr);
}

// Simulate each source timing out, then going back online before the notification can be delivered.
// In this case we should get no expired notification.
TEST_F(TestSourceLoss, EachSourceOfflineThenOnline)
{
  for (size_t i = 0; i < sources_.size(); ++i)
  {
    SacnLostSourceInternal offline{sources_[i].handle, sources_[i].name, false};
    if (i < sources_.size() - 1)
      mark_sources_offline(&offline, 1, &sources_[i + 1], sources_.size() - i - 1, &term_set_list_, 1000);
    else
      mark_sources_offline(&offline, 1, nullptr, 0, &term_set_list_, 1000);
    if (i > 0)
      mark_sources_online(sources_.data(), i, term_set_list_);

    // Advance time by 50ms
    etcpal_getms_fake.return_val += 50;
  }

  // All sources have gone back online
  mark_sources_online(sources_.data(), sources_.size(), term_set_list_);
  get_expired_sources(&term_set_list_, expired_sources_);
  EXPECT_EQ(expired_sources_->num_lost_sources, 0u);

  // Advance time to 1.1s
  etcpal_getms_fake.return_val = 1100;

  get_expired_sources(&term_set_list_, expired_sources_);
  EXPECT_EQ(expired_sources_->num_lost_sources, 0u);
  EXPECT_EQ(term_set_list_, nullptr);
}

TEST_F(TestSourceLoss, ClearListWorks)
{
  std::vector<SacnLostSourceInternal> offline;
  offline.reserve(SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);

  // Use the same setup as the Worst Case test. At the end of this, we should have the maximum
  // theoretical number of termination sets.
  for (size_t i = 0; i < sources_.size(); ++i)
  {
    offline.push_back(SacnLostSourceInternal{sources_[i].handle, sources_[i].name, false});
    if (i < sources_.size() - 1)
    {
      mark_sources_offline(offline.data(), offline.size(), &sources_[i + 1], sources_.size() - i - 1, &term_set_list_,
                           1000);
      mark_sources_online(&sources_[i + 1], sources_.size() - 1 - i, term_set_list_);
    }
    else
    {
      mark_sources_offline(offline.data(), offline.size(), nullptr, 0, &term_set_list_, 1000);
    }
    get_expired_sources(&term_set_list_, expired_sources_);
    EXPECT_EQ(expired_sources_->num_lost_sources, 0u);
    // Advance time by 50ms
    etcpal_getms_fake.return_val += 50;
  }

  EXPECT_NE(term_set_list_, nullptr);

  // Now clean up the list as if we had destroyed a receiver before resolving the termination sets.
  // Any cleanup failure should be caught by ASAN/leak checker.
  clear_term_set_list(term_set_list_);
}
