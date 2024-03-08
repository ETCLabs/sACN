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

// The primary goal of these tests is to execute as much sACN library code together as possible, including EtcPal. This
// way all of that code will be under the scrutiny of the sanitizers (e.g. ASAN, UBSAN, TSAN). So the focus here is less
// on correct behavior (that's for other tests to verify) and more on code execution.

#include "sacn/cpp/common.h"
#include "sacn/cpp/merge_receiver.h"
#include "sacn/cpp/source.h"
#include "sacn/cpp/source_detector.h"

#include "etcpal/cpp/netint.h"
#include "etcpal/cpp/signal.h"
#include "etcpal/cpp/thread.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <array>
#include <cstdlib>
#include <ctime>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "sacn_config.h"

using testing::_;
using testing::AtLeast;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

#if SACN_DYNAMIC_MEM
#define CoverageTest CoverageTestDynamic
#else
#define CoverageTest CoverageTestStatic
#endif

using UniverseId = uint16_t;

static constexpr UniverseId kDefaultUniverse = 1u;

class MockLogMessageHandler : public etcpal::LogMessageHandler
{
public:
  MockLogMessageHandler()
  {
    ON_CALL(*this, HandleLogMessage(_)).WillByDefault(Invoke([&](const EtcPalLogStrings& strings) {
      EXPECT_TRUE(false) << "LOG: " << strings.human_readable;
    }));
  }

  MOCK_METHOD(void, HandleLogMessage, (const EtcPalLogStrings& strings), (override));
};

class MockMergeReceiverNotifyHandler : public sacn::MergeReceiver::NotifyHandler
{
public:
  MockMergeReceiverNotifyHandler() = default;

  MOCK_METHOD(void, HandleMergedData, (sacn::MergeReceiver::Handle handle, const SacnRecvMergedData& merged_data),
              (override));
  MOCK_METHOD(void, HandleNonDmxData,
              (sacn::MergeReceiver::Handle receiver_handle, const etcpal::SockAddr& source_addr,
               const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data),
              (override));
  MOCK_METHOD(void, HandleSourcesLost,
              (sacn::MergeReceiver::Handle handle, uint16_t universe, const std::vector<SacnLostSource>& lost_sources),
              (override));
  MOCK_METHOD(void, HandleSamplingPeriodStarted, (sacn::MergeReceiver::Handle handle, uint16_t universe), (override));
  MOCK_METHOD(void, HandleSamplingPeriodEnded, (sacn::MergeReceiver::Handle handle, uint16_t universe), (override));
  MOCK_METHOD(void, HandleSourcePapLost,
              (sacn::MergeReceiver::Handle handle, uint16_t universe, const SacnRemoteSource& source), (override));
  MOCK_METHOD(void, HandleSourceLimitExceeded, (sacn::MergeReceiver::Handle handle, uint16_t universe), (override));
};

class MockSourceDetectorNotifyHandler : public sacn::SourceDetector::NotifyHandler
{
public:
  MockSourceDetectorNotifyHandler() = default;

  MOCK_METHOD(void, HandleSourceUpdated,
              (sacn::RemoteSourceHandle handle, const etcpal::Uuid& cid, const std::string& name,
               const std::vector<uint16_t>& sourced_universes),
              (override));
  MOCK_METHOD(void, HandleSourceExpired,
              (sacn::RemoteSourceHandle handle, const etcpal::Uuid& cid, const std::string& name), (override));
  MOCK_METHOD(void, HandleMemoryLimitExceeded, (), (override));
};

class TestMergeReceiver
{
public:
  struct UniverseChange
  {
    UniverseId from{kDefaultUniverse};
    UniverseId to;
  };

  TestMergeReceiver(sacn::McastMode initial_mcast_mode = sacn::McastMode::kEnabledOnAllInterfaces)
      : initial_mcast_mode_(initial_mcast_mode)
  {
  }

  ~TestMergeReceiver()
  {
    for (auto& [universe_id, state] : universes_)
      state->merge_receiver.Shutdown();
  }

  void AddUniverse(UniverseId universe_id = kDefaultUniverse)
  {
    EXPECT_EQ(universes_.find(universe_id), universes_.end());
    universes_[universe_id] = std::make_unique<UniverseState>();
  }

  void StartAllUniverses()
  {
    for (auto& [universe_id, state] : universes_)
    {
      EXPECT_TRUE(state->merge_receiver.Startup(sacn::MergeReceiver::Settings(universe_id), state->notify,
                                                initial_mcast_mode_));
    }
  }

  void ChangeUniverse(const UniverseChange& change)
  {
    ASSERT_NE(universes_.find(change.from), universes_.end());
    ASSERT_EQ(universes_.find(change.to), universes_.end());

    auto& state = universes_[change.to];
    state = std::move(universes_[change.from]);
    universes_.erase(change.from);

    EXPECT_TRUE(state->merge_receiver.ChangeUniverse(change.to));
  }

  MockMergeReceiverNotifyHandler& GetNotifyHandlerForUniverse(UniverseId universe_id = kDefaultUniverse)
  {
    EXPECT_NE(universes_.find(universe_id), universes_.end());
    return universes_[universe_id]->notify;
  }

private:
  struct UniverseState
  {
    sacn::MergeReceiver merge_receiver;
    NiceMock<MockMergeReceiverNotifyHandler> notify;
  };

  std::map<UniverseId, std::unique_ptr<UniverseState>> universes_;
  sacn::McastMode initial_mcast_mode_{sacn::McastMode::kEnabledOnAllInterfaces};
};

class TestSourceDetector
{
public:
  TestSourceDetector(sacn::McastMode initial_mcast_mode = sacn::McastMode::kEnabledOnAllInterfaces)
      : initial_mcast_mode_(initial_mcast_mode)
  {
  }
  ~TestSourceDetector() { sacn::SourceDetector::Shutdown(); }

  void Startup() { EXPECT_TRUE(sacn::SourceDetector::Startup(notify_, initial_mcast_mode_)); }
  MockSourceDetectorNotifyHandler& GetNotifyHandler() { return notify_; }

private:
  NiceMock<MockSourceDetectorNotifyHandler> notify_;
  sacn::McastMode initial_mcast_mode_{sacn::McastMode::kEnabledOnAllInterfaces};
};

class TestSource
{
public:
  struct StartCodeParams
  {
    int code;
    int value;
    std::optional<int> min;
    std::optional<int> max;
  };

  struct UniverseParams
  {
    uint16_t universe{kDefaultUniverse};
    uint8_t universe_priority{100u};
    std::vector<StartCodeParams> start_codes;
  };

  TestSource(sacn::McastMode initial_mcast_mode = sacn::McastMode::kEnabledOnAllInterfaces)
      : initial_mcast_mode_(initial_mcast_mode)
  {
    auto cid = etcpal::Uuid::V4();
    source_ = std::make_unique<SourceState>();
    EXPECT_TRUE(source_);
    if (source_)
    {
      EXPECT_TRUE(source_->source.Startup(sacn::Source::Settings(cid, std::string("Test Source ") + cid.ToString())));

      auto& source = *source_;  // Always track the SourceState object even if the TestSource moves
      source_->thread.Start([&source]() {
        while (!source.terminate.TryWait())
        {
          {
            etcpal::MutexGuard guard(source.universes_lock);
            for (auto& [id, state] : source.universes)
              UniverseTick(source.source, id, state);
          }
          etcpal::Thread::Sleep(kSleepMs);
        }
      });
    }
  }

  TestSource(const TestSource& rhs) = delete;
  TestSource(TestSource&& rhs) noexcept = default;

  ~TestSource()
  {
    if (source_)  // Could be empty if moved
    {
      source_->terminate.Notify();
      source_->thread.Join();
      source_->source.Shutdown();
    }
  }

  void AddUniverse(const UniverseParams& params)
  {
    ASSERT_TRUE(source_);
    if (source_)
    {
      etcpal::MutexGuard guard(source_->universes_lock);

      ASSERT_EQ(source_->universes.find(params.universe), source_->universes.end());

      auto& state = source_->universes[params.universe];

      for (const auto& start_code : params.start_codes)
      {
        ASSERT_TRUE((start_code.code == SACN_STARTCODE_DMX) || (start_code.code == SACN_STARTCODE_PRIORITY));
        switch (start_code.code)
        {
          case SACN_STARTCODE_DMX:
            state.null_start_code = StartCodeState(start_code);
            break;
          case SACN_STARTCODE_PRIORITY:
            state.pap_start_code = StartCodeState(start_code);
            break;
        }
      }

      sacn::Source::UniverseSettings settings;
      settings.universe = params.universe;
      settings.priority = params.universe_priority;

      EXPECT_TRUE(source_->source.AddUniverse(settings, initial_mcast_mode_));

      UniverseTick(source_->source, params.universe, state);  // Initialize values sooner by ticking the universe here
    }
  }

  void RemoveUniverse(UniverseId universe_id = kDefaultUniverse)
  {
    ASSERT_TRUE(source_);
    if (source_)
    {
      etcpal::MutexGuard guard(source_->universes_lock);

      ASSERT_NE(source_->universes.find(universe_id), source_->universes.end());
      source_->universes.erase(universe_id);
      source_->source.RemoveUniverse(universe_id);
    }
  }

private:
  static constexpr unsigned int kSleepMs = 500u;

  struct StartCodeState
  {
    StartCodeState() = default;
    StartCodeState(const StartCodeParams& p) : params(p) { buffer.fill(static_cast<uint8_t>(p.value)); }

    StartCodeParams params;
    std::array<uint8_t, 10> buffer{};
    bool uninitialized{true};
  };

  struct UniverseState
  {
    StartCodeState null_start_code;
    std::optional<StartCodeState> pap_start_code;
  };

  struct SourceState
  {
    sacn::Source source;
    std::unordered_map<UniverseId, UniverseState> universes;

    etcpal::Thread thread;
    etcpal::Signal terminate;
    etcpal::Mutex universes_lock;
  };

  static void UniverseTick(sacn::Source& source, UniverseId universe_id, UniverseState& state)
  {
    bool updated_null = UpdateStartCodeData(state.null_start_code);
    bool updated_pap = state.pap_start_code && UpdateStartCodeData(*state.pap_start_code);
    if (updated_null || updated_pap)
    {
      if (state.pap_start_code)
      {
        source.UpdateLevelsAndPap(universe_id, state.null_start_code.buffer.data(), state.null_start_code.buffer.size(),
                                  state.pap_start_code->buffer.data(), state.pap_start_code->buffer.size());
      }
      else
      {
        source.UpdateLevels(universe_id, state.null_start_code.buffer.data(), state.null_start_code.buffer.size());
      }
    }
  }

  static bool UpdateStartCodeData(StartCodeState& state)
  {
    if (state.params.min && state.params.max)
    {
      for (uint8_t& slot : state.buffer)
        slot = static_cast<uint8_t>(rand() % (*state.params.max - *state.params.min + 1) + *state.params.min);

      return true;
    }

    if (state.uninitialized)
    {
      state.uninitialized = false;
      return true;
    }

    return false;
  }

  std::unique_ptr<SourceState> source_;
  sacn::McastMode initial_mcast_mode_{sacn::McastMode::kEnabledOnAllInterfaces};
};

class CoverageTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    srand(static_cast<unsigned int>(time(nullptr)));

    EXPECT_TRUE(logger_.Startup(mock_log_handler_));
    EXPECT_TRUE(sacn::Init(logger_));
  }

  void TearDown() override
  {
    sacn::Deinit();
    logger_.Shutdown();
  }

  void ResetNetworking()
  {
    // Do this asynchronously to catch any data races between API boundaries
    etcpal::Thread merge_receiver_reset_thread;
    // etcpal::Thread source_detector_reset_thread;
    etcpal::Thread source_reset_thread;

    merge_receiver_reset_thread.Start([]() { sacn::MergeReceiver::ResetNetworking(); });
    // source_detector_reset_thread.Start([]() { sacn::SourceDetector::ResetNetworking(); });
    source_reset_thread.Start([]() { sacn::Source::ResetNetworking(); });

    merge_receiver_reset_thread.Join();
    // source_detector_reset_thread.Join();
    source_reset_thread.Join();
  }

  etcpal::Logger logger_;

  NiceMock<MockLogMessageHandler> mock_log_handler_;
};

TEST_F(CoverageTest, ResetNetworkingAtScale)
{
  static std::map<uint16_t, int> num_per_universe;
  TestMergeReceiver merge_receiver;
  for (uint16_t universe_id = 1u; universe_id <= 15u; ++universe_id)
  {
    merge_receiver.AddUniverse(universe_id);
    EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(universe_id), HandleMergedData(_, _)).Times(AtLeast(1));

    ON_CALL(merge_receiver.GetNotifyHandlerForUniverse(universe_id), HandleMergedData(_, _))
        .WillByDefault(Invoke([&](sacn::MergeReceiver::Handle handle, const SacnRecvMergedData& merged_data) {
          ++num_per_universe[merged_data.universe_id];
        }));
  }

  merge_receiver.StartAllUniverses();

  TestSource source;
  for (uint16_t universe_id = 1u; universe_id <= 100u; ++universe_id)
    source.AddUniverse({.universe = universe_id, .start_codes = {{.code = SACN_STARTCODE_DMX, .value = 0xFF}}});

  etcpal::Thread::Sleep(11000u);  // Time for source detector to detect sources

  for (auto [univ, count] : num_per_universe)
    EXPECT_TRUE(false) << "UNIVERSE " << univ << " COUNT " << count;
}
