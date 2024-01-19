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

#include <unordered_set>

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

#if SACN_DYNAMIC_MEM
#define CoverageTest CoverageTestDynamic
#else
#define CoverageTest CoverageTestStatic
#endif

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

class TestSource
{
public:
  struct ValueRange
  {
    ValueRange() = default;
    ValueRange(int val) : min(val), max(val) {}

    int min{0};
    int max{0};
  };

  struct StartCodeParams
  {
    uint8_t code{0u};
    ValueRange value;
  };

  struct UniverseParams
  {
    std::vector<StartCodeParams> start_codes;
    int updates_per_second{300};
  };

  TestSource() = default;

  void AddUniverse(const UniverseParams& universe_params);

private:
  struct UniverseState
  {
    UniverseParams params;
    etcpal::Thread thread;
    etcpal::Signal terminate;
  };

  sacn::Source source_;
  std::vector<UniverseState> universes;
};

class CoverageTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    EXPECT_TRUE(logger_.Startup(mock_log_handler_));
    EXPECT_TRUE(sacn::Init(logger_));
  }

  void TearDown() override
  {
    sacn::Deinit();
    logger_.Shutdown();
  }

  etcpal::Logger logger_;

  std::vector<TestSource> sources_;

  NiceMock<MockLogMessageHandler> mock_log_handler_;
};

TEST_F(CoverageTest, SourceToMergeReceiverAndSourceDetector)
{
  // TODO
}
