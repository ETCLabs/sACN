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

bool WaitForSignal(etcpal::Signal& signal, uint32_t or_until_ms_elapsed)
{
  static constexpr uint32_t kWaitIntervalMs = 1000u;

  for (uint32_t elapsed_ms = 0u; elapsed_ms < or_until_ms_elapsed; elapsed_ms += kWaitIntervalMs)
  {
    if (signal.TryWait())
      return true;

    etcpal::Thread::Sleep(kWaitIntervalMs);
  }

  return false;
}

class MockLogMessageHandler : public etcpal::LogMessageHandler
{
public:
  MockLogMessageHandler()                                              = default;
  MockLogMessageHandler(const MockLogMessageHandler& other)            = delete;
  MockLogMessageHandler& operator=(const MockLogMessageHandler& other) = delete;
  MockLogMessageHandler(MockLogMessageHandler&& other)                 = delete;
  MockLogMessageHandler& operator=(MockLogMessageHandler&& other)      = delete;
  virtual ~MockLogMessageHandler()                                     = default;

  MOCK_METHOD(void, HandleLogMessage, (const EtcPalLogStrings& strings), (override));
};

class MockMergeReceiverNotifyHandler : public sacn::MergeReceiver::NotifyHandler
{
public:
  MockMergeReceiverNotifyHandler() = default;

  MOCK_METHOD(void,
              HandleMergedData,
              (sacn::MergeReceiver::Handle handle, const SacnRecvMergedData& merged_data),
              (override));
  MOCK_METHOD(void,
              HandleNonDmxData,
              (sacn::MergeReceiver::Handle receiver_handle,
               const etcpal::SockAddr&     source_addr,
               const SacnRemoteSource&     source_info,
               const SacnRecvUniverseData& universe_data),
              (override));
  MOCK_METHOD(void,
              HandleSourcesLost,
              (sacn::MergeReceiver::Handle handle, uint16_t universe, const std::vector<SacnLostSource>& lost_sources),
              (override));
  MOCK_METHOD(void, HandleSamplingPeriodStarted, (sacn::MergeReceiver::Handle handle, uint16_t universe), (override));
  MOCK_METHOD(void, HandleSamplingPeriodEnded, (sacn::MergeReceiver::Handle handle, uint16_t universe), (override));
  MOCK_METHOD(void,
              HandleSourcePapLost,
              (sacn::MergeReceiver::Handle handle, uint16_t universe, const SacnRemoteSource& source),
              (override));
  MOCK_METHOD(void, HandleSourceLimitExceeded, (sacn::MergeReceiver::Handle handle, uint16_t universe), (override));
};

class MockSourceDetectorNotifyHandler : public sacn::SourceDetector::NotifyHandler
{
public:
  MockSourceDetectorNotifyHandler()
  {
    ON_CALL(*this, HandleSourceUpdated(_, _, _, _))
        .WillByDefault(Invoke([&](sacn::RemoteSourceHandle, const etcpal::Uuid&, const std::string&,
                                  const std::vector<uint16_t>&) { handle_source_updated_signal_.Notify(); }));
    ON_CALL(*this, HandleSourceExpired(_, _, _))
        .WillByDefault(Invoke([&](sacn::RemoteSourceHandle, const etcpal::Uuid&, const std::string&) {
          handle_source_expired_signal_.Notify();
        }));
  }

  MOCK_METHOD(void,
              HandleSourceUpdated,
              (sacn::RemoteSourceHandle     handle,
               const etcpal::Uuid&          cid,
               const std::string&           name,
               const std::vector<uint16_t>& sourced_universes),
              (override));
  MOCK_METHOD(void,
              HandleSourceExpired,
              (sacn::RemoteSourceHandle handle, const etcpal::Uuid& cid, const std::string& name),
              (override));
  MOCK_METHOD(void, HandleMemoryLimitExceeded, (), (override));

  bool WaitForSourceUpdated(uint32_t or_until_ms_elapsed)
  {
    bool res = WaitForSignal(handle_source_updated_signal_, or_until_ms_elapsed);
    etcpal::Thread::Sleep(3000u);  // A bit extra for other sources
    return res;
  }

  bool WaitForSourceExpired(uint32_t or_until_ms_elapsed)
  {
    bool res = WaitForSignal(handle_source_expired_signal_, or_until_ms_elapsed);
    etcpal::Thread::Sleep(3000u);  // A bit extra for other sources
    return res;
  }

private:
  etcpal::Signal handle_source_updated_signal_;
  etcpal::Signal handle_source_expired_signal_;
};

class TestMergeReceiver
{
public:
  struct UniverseChange
  {
    UniverseId from{kDefaultUniverse};
    UniverseId to{kDefaultUniverse};
  };

  TestMergeReceiver(sacn::McastMode initial_mcast_mode = sacn::McastMode::kEnabledOnAllInterfaces)
      : initial_mcast_mode_(initial_mcast_mode)
  {
  }

  TestMergeReceiver(const TestMergeReceiver& other)            = default;
  TestMergeReceiver& operator=(const TestMergeReceiver& other) = default;
  TestMergeReceiver(TestMergeReceiver&& other)                 = default;
  TestMergeReceiver& operator=(TestMergeReceiver&& other)      = default;

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
    state       = std::move(universes_[change.from]);
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
    sacn::MergeReceiver                      merge_receiver;
    NiceMock<MockMergeReceiverNotifyHandler> notify;
  };

  std::unordered_map<UniverseId, std::unique_ptr<UniverseState>> universes_;
  sacn::McastMode initial_mcast_mode_{sacn::McastMode::kEnabledOnAllInterfaces};
};

class TestSourceDetector
{
public:
  TestSourceDetector(sacn::McastMode initial_mcast_mode = sacn::McastMode::kEnabledOnAllInterfaces)
      : initial_mcast_mode_(initial_mcast_mode)
  {
  }

  TestSourceDetector(const TestSourceDetector& other)            = delete;
  TestSourceDetector& operator=(const TestSourceDetector& other) = delete;
  TestSourceDetector(TestSourceDetector&& other)                 = default;
  TestSourceDetector& operator=(TestSourceDetector&& other)      = delete;

  ~TestSourceDetector() { sacn::SourceDetector::Shutdown(); }

  void Startup() { EXPECT_TRUE(sacn::SourceDetector::Startup(notify_, initial_mcast_mode_)); }
  MockSourceDetectorNotifyHandler& GetNotifyHandler() { return notify_; }

private:
  NiceMock<MockSourceDetectorNotifyHandler> notify_;
  sacn::McastMode                           initial_mcast_mode_{sacn::McastMode::kEnabledOnAllInterfaces};
};

class TestSource
{
public:
  struct StartCodeParams
  {
    int                code{};
    int                value{};
    std::optional<int> min;
    std::optional<int> max;
  };

  struct UniverseParams
  {
    uint16_t                     universe{kDefaultUniverse};
    uint8_t                      universe_priority{100u};
    std::vector<StartCodeParams> start_codes;
  };

  TestSource(sacn::McastMode initial_mcast_mode = sacn::McastMode::kEnabledOnAllInterfaces)
      : initial_mcast_mode_(initial_mcast_mode)
  {
    auto cid = etcpal::Uuid::V4();
    source_  = std::make_unique<SourceState>();
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

  TestSource(const TestSource& rhs)              = delete;
  TestSource& operator=(const TestSource& other) = delete;
  TestSource(TestSource&& rhs) noexcept          = default;
  TestSource& operator=(TestSource&& other)      = default;

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
        ASSERT_TRUE((start_code.code == kSacnStartcodeDmx) || (start_code.code == kSacnStartcodePriority));
        switch (start_code.code)
        {
          case kSacnStartcodeDmx:
            state.null_start_code = StartCodeState(start_code);
            break;
          case kSacnStartcodePriority:
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
    StartCodeState(const StartCodeParams& start_code_params) : params(start_code_params)
    {
      buffer.fill(static_cast<uint8_t>(start_code_params.value));
    }

    StartCodeParams                           params;
    std::array<uint8_t, kSacnDmxAddressCount> buffer{};
    bool                                      uninitialized{true};
  };

  struct UniverseState
  {
    StartCodeState                null_start_code;
    std::optional<StartCodeState> pap_start_code;
  };

  struct SourceState
  {
    sacn::Source                                  source;
    std::unordered_map<UniverseId, UniverseState> universes;

    etcpal::Thread thread;
    etcpal::Signal terminate;
    etcpal::Mutex  universes_lock;
  };

  static void UniverseTick(sacn::Source& source, UniverseId universe_id, UniverseState& state)
  {
    bool updated_null = UpdateStartCodeData(state.null_start_code);
    bool updated_pap  = state.pap_start_code && UpdateStartCodeData(*state.pap_start_code);
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
  sacn::McastMode              initial_mcast_mode_{sacn::McastMode::kEnabledOnAllInterfaces};
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

  static void ResetNetworking()
  {
    // Do this asynchronously to catch any data races between API boundaries
    etcpal::Thread merge_receiver_reset_thread;
    etcpal::Thread source_detector_reset_thread;
    etcpal::Thread source_reset_thread;

    merge_receiver_reset_thread.Start([]() { sacn::MergeReceiver::ResetNetworking(); });
    source_detector_reset_thread.Start([]() { sacn::SourceDetector::ResetNetworking(); });
    source_reset_thread.Start([]() { sacn::Source::ResetNetworking(); });

    merge_receiver_reset_thread.Join();
    source_detector_reset_thread.Join();
    source_reset_thread.Join();
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

  merge_receiver.StartAllUniverses();

  TestSource source;
  source.AddUniverse({.start_codes = {{.code = kSacnStartcodeDmx, .value = 0xFF}}});

  etcpal::Thread::Sleep(2000u);  // Cover sampling period
}

TEST_F(CoverageTest, SendReceiveAndMergeAtScale)
{
  static constexpr int                                     kNumTestSources = 7;
  static constexpr std::array<UniverseId, kNumTestSources> kTestUniverses  = {1u, 2u, 3u, 4u, 5u, 6u, 7u};

  TestMergeReceiver merge_receiver;
  for (auto universe_id : kTestUniverses)
  {
    merge_receiver.AddUniverse(universe_id);
    EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(universe_id), HandleMergedData(_, _)).Times(AtLeast(1));
    EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(universe_id), HandleSamplingPeriodStarted(_, _))
        .Times(AtLeast(1));
    EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(universe_id), HandleSamplingPeriodEnded(_, _))
        .Times(AtLeast(1));
  }

  merge_receiver.StartAllUniverses();

  std::vector<TestSource> sources;
  for (int i = 0; i < kNumTestSources; ++i)
  {
    TestSource source;
    for (UniverseId universe_id : kTestUniverses)
    {
      source.AddUniverse({.universe    = universe_id,
                          .start_codes = {{.code = kSacnStartcodeDmx, .min = 0x00, .max = 0xFF},
                                          {.code = kSacnStartcodePriority, .min = 0x00, .max = 0xFF}}});
    }

    sources.push_back(std::move(source));
  }

  etcpal::Thread::Sleep(2000u);  // Cover sampling period
}

TEST_F(CoverageTest, SwitchThroughUniverses)
{
  static constexpr int                                       kNumTestUniverses = 3;
  static constexpr std::array<UniverseId, kNumTestUniverses> kTestUniverses    = {1u, 2u, 3u};

  TestMergeReceiver merge_receiver;
  merge_receiver.AddUniverse(kTestUniverses[0]);
  EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(kTestUniverses[0]), HandleMergedData(_, _)).Times(AtLeast(1));
  EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(kTestUniverses[0]), HandleSamplingPeriodStarted(_, _))
      .Times(kNumTestUniverses);
  EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(kTestUniverses[0]), HandleSamplingPeriodEnded(_, _))
      .Times(kNumTestUniverses);

  merge_receiver.StartAllUniverses();

  TestSource source;
  for (auto universe_id : kTestUniverses)
    source.AddUniverse({.universe = universe_id, .start_codes = {{.code = kSacnStartcodeDmx, .value = 0xFF}}});

  for (int i = 0, j = 1; j < kNumTestUniverses; ++i, ++j)
  {
    etcpal::Thread::Sleep(2000u);  // Cover sampling period
    merge_receiver.ChangeUniverse({.from = kTestUniverses.at(i), .to = kTestUniverses.at(j)});
  }

  etcpal::Thread::Sleep(2000u);  // Cover sampling period
}

TEST_F(CoverageTest, DetectSourcesComingAndGoing)
{
  static constexpr uint32_t kWorstCaseWaitMs = 300000u;

  static constexpr int                                     kNumTestSources = 7;
  static constexpr std::array<UniverseId, kNumTestSources> kTestUniverses  = {1u, 2u, 3u, 4u, 5u, 6u, 7u};

  TestSourceDetector source_detector;
  EXPECT_CALL(source_detector.GetNotifyHandler(), HandleSourceUpdated(_, _, _, _)).Times(AtLeast(1));
  EXPECT_CALL(source_detector.GetNotifyHandler(), HandleSourceExpired(_, _, _)).Times(AtLeast(1));
  source_detector.Startup();

  std::vector<TestSource> sources;
  for (int i = 0; i < kNumTestSources; ++i)
  {
    TestSource source;
    for (auto universe_id : kTestUniverses)
      source.AddUniverse({.universe = universe_id, .start_codes = {{.code = kSacnStartcodeDmx, .value = 0xFF}}});

    sources.push_back(std::move(source));
  }

  EXPECT_TRUE(source_detector.GetNotifyHandler().WaitForSourceUpdated(kWorstCaseWaitMs));

  for (int i = 0; i < kNumTestSources; ++i)
    sources.pop_back();

  EXPECT_TRUE(source_detector.GetNotifyHandler().WaitForSourceExpired(kWorstCaseWaitMs));
}

TEST_F(CoverageTest, ResetNetworkingAtScale)
{
  static constexpr int kNumUniverses = 25;
  static constexpr int kNumSources   = 2;

  auto sys_netints = etcpal::netint::GetInterfaces();
  ASSERT_TRUE(sys_netints);

  TestMergeReceiver merge_receiver(sacn::McastMode::kDisabledOnAllInterfaces);
  for (uint16_t universe_id = 1u; universe_id <= kNumUniverses; ++universe_id)
  {
    merge_receiver.AddUniverse(universe_id);
    EXPECT_CALL(merge_receiver.GetNotifyHandlerForUniverse(universe_id), HandleMergedData(_, _)).Times(AtLeast(1));
  }

  merge_receiver.StartAllUniverses();

  TestSourceDetector source_detector(sacn::McastMode::kDisabledOnAllInterfaces);
  EXPECT_CALL(source_detector.GetNotifyHandler(), HandleSourceUpdated(_, _, _, _)).Times(AtLeast(1));
  source_detector.Startup();

  std::vector<TestSource> sources;
  for (int i = 0; i < kNumSources; ++i)
  {
    TestSource source(sacn::McastMode::kDisabledOnAllInterfaces);
    for (uint16_t universe_id = 1u; universe_id <= kNumUniverses; ++universe_id)
      source.AddUniverse({.universe = universe_id, .start_codes = {{.code = kSacnStartcodeDmx, .value = 0xFF}}});

    sources.push_back(std::move(source));
  }

  std::vector<SacnMcastInterface> netints = {};
  size_t add_until_this_many_netints_left = sys_netints->size() / 2;  // First, add half of the netints
  while (!sys_netints->empty())
  {
    etcpal::Thread::Sleep(1000u);  // Allow for some network activity each time

    while (sys_netints->size() > add_until_this_many_netints_left)
    {
      netints.push_back(
          {.iface = {.ip_type = sys_netints->back().addr().get().type, .index = sys_netints->back().index().value()}});
      sys_netints->pop_back();

      add_until_this_many_netints_left = 0u;  // Next time add the other half
    }

    ResetNetworking();

    netints.clear();  // Try each half individually
  }

  // One last reset, this time with all netints
  etcpal::Thread::Sleep(1000u);

  ResetNetworking();

  etcpal::Thread::Sleep(11000u);  // Time for source detector to detect sources
}
