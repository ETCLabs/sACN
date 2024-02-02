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

#include "etcpal/cpp/thread.h"
#include "etcpal/cpp/signal.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <array>
#include <cstdlib>
#include <ctime>
#include <unordered_map>
#include <unordered_set>

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
  MockLogMessageHandler() = default;

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
  TestMergeReceiver() = default;

  ~TestMergeReceiver()
  {
    for (auto& [universe_id, state] : universes_)
      state.merge_receiver.Shutdown();
  }

  void AddUniverse(UniverseId universe_id = kDefaultUniverse)
  {
    ASSERT_EQ(universes_.find(universe_id), universes_.end());

    UniverseState& state = universes_[universe_id];
    EXPECT_TRUE(state.merge_receiver.Startup(sacn::MergeReceiver::Settings(universe_id), state.notify));
  }

  MockMergeReceiverNotifyHandler& GetNotifyHandlerForUniverse(UniverseId universe_id = kDefaultUniverse)
  {
    EXPECT_NE(universes_.find(universe_id), universes_.end());
    return universes_[universe_id].notify;
  }

private:
  struct UniverseState
  {
    sacn::MergeReceiver merge_receiver;
    NiceMock<MockMergeReceiverNotifyHandler> notify;
  };

  std::unordered_map<UniverseId, UniverseState> universes_;
};

class TestSourceDetector
{
public:
  TestSourceDetector() = default;
  ~TestSourceDetector() { sacn::SourceDetector::Shutdown(); }

  void Startup() { EXPECT_TRUE(sacn::SourceDetector::Startup(notify_)); }
  MockSourceDetectorNotifyHandler& GetNotifyHandler() { return notify_; }

private:
  NiceMock<MockSourceDetectorNotifyHandler> notify_;
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

  TestSource()
  {
    auto cid = etcpal::Uuid::V4();
    EXPECT_TRUE(source_.Startup(sacn::Source::Settings(cid, std::string("Test Source ") + cid.ToString())));
  }

  ~TestSource()
  {
    for (auto& [universe_id, state] : universes_)
      state.terminate.Notify();

    for (auto& [universe_id, state] : universes_)
      state.thread.Join();

    source_.Shutdown();
  }

  void AddUniverse(const UniverseParams& params)
  {
    ASSERT_EQ(universes_.find(params.universe), universes_.end());

    UniverseState& state = universes_[params.universe];

    for (const auto& start_code : params.start_codes)
    {
      switch (start_code.code)
      {
        case SACN_STARTCODE_DMX:
          state.null_start_code = StartCodeState(start_code);
          break;
        case SACN_STARTCODE_PRIORITY:
          state.pap_start_code = StartCodeState(start_code);
          break;
        default:
          state.custom_start_codes.emplace_back(start_code);
      }
    }

    sacn::Source::UniverseSettings settings;
    settings.universe = params.universe;
    settings.priority = params.universe_priority;
    EXPECT_TRUE(source_.AddUniverse(settings));

    state.thread.Start([this, params, &state]() {
      while (!state.terminate.TryWait())
      {
        UniverseTick(params.universe, state);
        etcpal::Thread::Sleep(kUniverseSleepMs);
      }
    });
  }

  void RemoveUniverse(UniverseId universe_id = kDefaultUniverse)
  {
    ASSERT_NE(universes_.find(universe_id), universes_.end());

    universes_[universe_id].terminate.Notify();
    universes_[universe_id].thread.Join();

    universes_.erase(universe_id);

    source_.RemoveUniverse(universe_id);
  }

private:
  static constexpr unsigned int kUniverseSleepMs = 100u;

  struct StartCodeState
  {
    StartCodeState() = default;
    StartCodeState(const StartCodeParams& p) : params(p) { buffer.fill(static_cast<uint8_t>(p.value)); }

    StartCodeParams params;
    std::array<uint8_t, DMX_ADDRESS_COUNT> buffer{};
  };

  struct UniverseState
  {
    StartCodeState null_start_code;
    std::optional<StartCodeState> pap_start_code;
    std::vector<StartCodeState> custom_start_codes;
    etcpal::Thread thread;
    etcpal::Signal terminate;
  };

  void UniverseTick(UniverseId universe_id, UniverseState& state)
  {
    UpdateStartCodeData(state.null_start_code);
    if (state.pap_start_code)
    {
      UpdateStartCodeData(*state.pap_start_code);
      source_.UpdateLevelsAndPap(universe_id, state.null_start_code.buffer.data(), state.null_start_code.buffer.size(),
                                 state.pap_start_code->buffer.data(), state.pap_start_code->buffer.size());
    }
    else
    {
      source_.UpdateLevels(universe_id, state.null_start_code.buffer.data(), state.null_start_code.buffer.size());
    }

    for (auto& custom_code : state.custom_start_codes)
    {
      UpdateStartCodeData(custom_code);
      source_.SendNow(universe_id, static_cast<uint8_t>(custom_code.params.code), custom_code.buffer.data(),
                      custom_code.buffer.size());
    }
  }

  void UpdateStartCodeData(StartCodeState& state)
  {
    if (state.params.min && state.params.max)
    {
      for (uint8_t& slot : state.buffer)
        slot = static_cast<uint8_t>(rand() % (*state.params.max - *state.params.min + 1) + *state.params.min);
    }
  }

  sacn::Source source_;
  std::unordered_map<UniverseId, UniverseState> universes_;
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

  etcpal::Logger logger_;

  NiceMock<MockLogMessageHandler> mock_log_handler_;
};

TEST_F(CoverageTest, SendAndReceiveSimpleUniverse)
{
  TestMergeReceiver merge_receiver;
  merge_receiver.AddUniverse();

  EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(), HandleMergedData(_, _)).Times(AtLeast(1));
  EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(), HandleSamplingPeriodStarted(_, _)).Times(AtLeast(1));
  EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(), HandleSamplingPeriodEnded(_, _)).Times(AtLeast(1));

  TestSource source;
  source.AddUniverse({.start_codes = {{.code = SACN_STARTCODE_DMX, .value = 0xFF}}});

  etcpal::Thread::Sleep(2000u);  // Cover sampling period
}

TEST_F(CoverageTest, SendReceiveAndMergeAtScale)
{
  static constexpr UniverseId kTestUniverses[] = {1u, 2u, 3u, 4u, 5u, 6u, 7u};
  static constexpr int kNumTestSources = 7u;

  TestMergeReceiver merge_receiver;
  for (UniverseId universe_id : kTestUniverses)
  {
    merge_receiver.AddUniverse(universe_id);
    EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(universe_id), HandleMergedData(_, _)).Times(AtLeast(1));
    EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(universe_id), HandleSamplingPeriodStarted(_, _))
        .Times(AtLeast(1));
    EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(universe_id), HandleSamplingPeriodEnded(_, _))
        .Times(AtLeast(1));
  }

  std::vector<std::unique_ptr<TestSource>> sources;
  for (int i = 0; i < kNumTestSources; ++i)
  {
    auto source = std::make_unique<TestSource>();
    ASSERT_TRUE(source);
    for (UniverseId universe_id : kTestUniverses)
    {
      source->AddUniverse({.universe = universe_id,
                           .start_codes = {{.code = SACN_STARTCODE_DMX, .min = 0x00, .max = 0xFF},
                                           {.code = SACN_STARTCODE_PRIORITY, .min = 0x00, .max = 0xFF}}});
    }

    sources.push_back(std::move(source));
  }

  etcpal::Thread::Sleep(2000u);  // Cover sampling period
}

#if 0  // TODO: WIP - fix sacn_config.h issue (sACN is being built and linked separately)

TEST_F(CoverageTest, DetectSourcesComingAndGoing)
{
  static constexpr UniverseId kTestUniverses[] = {1u, 2u, 3u, 4u, 5u, 6u, 7u};
  static constexpr int kNumTestSources = 7u;

  TestSourceDetector source_detector;
  source_detector.Startup();
  EXPECT_CALL(source_detector.GetNotifyHandler(), HandleSourceUpdated(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(source_detector.GetNotifyHandler(), HandleSourceExpired(_, _, _)).Times(AtLeast(1));

  std::vector<std::unique_ptr<TestSource>> sources;
  for (int i = 0; i < kNumTestSources; ++i)
  {
    auto source = std::make_unique<TestSource>();
    ASSERT_TRUE(source);
    for (UniverseId universe_id : kTestUniverses)
      source->AddUniverse({.universe = universe_id, .start_codes = {{.code = SACN_STARTCODE_DMX, .value = 0xFF}}});

    sources.push_back(std::move(source));
  }

  etcpal::Thread::Sleep(500u);  // Some time to detect sources

  for (int i = 0; i < kNumTestSources; ++i)
    sources.pop_back();

  etcpal::Thread::Sleep(21000u);  // Cover universe discovery expiration
}

#endif
