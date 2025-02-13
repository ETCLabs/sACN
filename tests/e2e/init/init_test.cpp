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

#include "sacn/cpp/common.h"
#include "sacn/cpp/dmx_merger.h"
#include "sacn/cpp/receiver.h"
#include "sacn/cpp/merge_receiver.h"
#include "sacn/cpp/source.h"
#include "sacn/cpp/source_detector.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <array>

#include "sacn_config.h"

using testing::_;
using testing::AtLeast;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

#if SACN_DYNAMIC_MEM
#define InitTest InitTestDynamic
#else
#define InitTest InitTestStatic
#endif

using UniverseId = uint16_t;

static constexpr sacn_features_t kNoFeatures       = 0u;
static constexpr sacn_features_t kAllOtherFeatures = (SACN_FEATURES_ALL & ~SACN_FEATURE_DMX_MERGER);

static constexpr UniverseId                                     kTestUniverse    = 123u;
static constexpr uint8_t                                        kTestPriority    = 123u;
static constexpr uint8_t                                        kTestStartCode   = 123u;
static constexpr std::array<uint8_t, SACN_DMX_MERGER_MAX_SLOTS> kTestValues      = {};
static constexpr uint32_t                                       kTestExpiredWait = 123u;
static constexpr const char*                                    kTestName        = "Test Name";
static const etcpal::IpAddr                                     kTestAddr = etcpal::IpAddr::FromString("10.101.1.1");

class MockReceiverNotifyHandler : public sacn::Receiver::NotifyHandler
{
public:
  MockReceiverNotifyHandler() = default;

  MOCK_METHOD(void,
              HandleUniverseData,
              (sacn::Receiver::Handle      handle,
               const etcpal::SockAddr&     source_addr,
               const SacnRemoteSource&     source_info,
               const SacnRecvUniverseData& universe_data),
              (override));
  MOCK_METHOD(void,
              HandleSourcesLost,
              (sacn::Receiver::Handle handle, uint16_t universe, const std::vector<SacnLostSource>& lost_sources),
              (override));
  MOCK_METHOD(void, HandleSamplingPeriodStarted, (sacn::Receiver::Handle handle, uint16_t universe), (override));
  MOCK_METHOD(void, HandleSamplingPeriodEnded, (sacn::Receiver::Handle handle, uint16_t universe), (override));
  MOCK_METHOD(void,
              HandleSourcePapLost,
              (sacn::Receiver::Handle handle, uint16_t universe, const SacnRemoteSource& source),
              (override));
  MOCK_METHOD(void, HandleSourceLimitExceeded, (sacn::Receiver::Handle handle, uint16_t universe), (override));
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
  MockSourceDetectorNotifyHandler() = default;

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
};

class InitTest : public ::testing::Test
{
protected:
  void VerifyInit(sacn_features_t features)
  {
    VerifyDmxMergerInit(features);
    VerifyReceiverInit(features);
    VerifyMergeReceiverInit(features);
    VerifySourceInit(features);
    VerifySourceDetectorInit(features);
  }

  void VerifyDmxMergerInit(sacn_features_t features)
  {
    sacn::DmxMerger merger;

    std::array<uint8_t, kSacnDmxAddressCount> levels = {};
    sacn::DmxMerger::Settings                 settings;
    settings.levels = levels.data();

    if ((features & SACN_FEATURE_DMX_MERGER) != 0u)
    {
      EXPECT_TRUE(merger.Startup(settings));

      auto source = merger.AddSource();
      ASSERT_TRUE(source);

      EXPECT_TRUE(merger.UpdateLevels(*source, kTestValues.data(), kTestValues.size()));
      EXPECT_TRUE(merger.UpdatePap(*source, kTestValues.data(), kTestValues.size()));
      EXPECT_TRUE(merger.UpdateUniversePriority(*source, kTestPriority));
      EXPECT_TRUE(merger.RemovePap(*source));
      EXPECT_TRUE(merger.RemoveSource(*source));
      merger.Shutdown();
    }
    else
    {
      EXPECT_EQ(merger.Startup(settings), kEtcPalErrNotInit);
      EXPECT_EQ(merger.AddSource(), kEtcPalErrNotInit);
    }
  }

  void VerifyReceiverInit(sacn_features_t features)
  {
    sacn::Receiver                      receiver;
    NiceMock<MockReceiverNotifyHandler> notify;

    sacn::Receiver::Settings settings;
    settings.universe_id = kTestUniverse;

    if ((features & kAllOtherFeatures) == kAllOtherFeatures)
    {
      EXPECT_TRUE(receiver.Startup(settings, notify));

      EXPECT_EQ(receiver.GetUniverse(), kTestUniverse);
      EXPECT_TRUE(receiver.ChangeUniverse(kTestUniverse + 1u));

      EXPECT_TRUE(receiver.ResetNetworking());

      sacn::Receiver::SetExpiredWait(kTestExpiredWait);
      EXPECT_EQ(sacn::Receiver::GetExpiredWait(), kTestExpiredWait);

      receiver.Shutdown();
    }
    else
    {
      EXPECT_EQ(receiver.Startup(settings, notify), kEtcPalErrNotInit);

      EXPECT_EQ(receiver.GetUniverse(), kEtcPalErrNotInit);
      EXPECT_EQ(receiver.ChangeUniverse(kTestUniverse + 1u), kEtcPalErrNotInit);

      EXPECT_EQ(receiver.ResetNetworking(), kEtcPalErrNotInit);

      sacn::Receiver::SetExpiredWait(kTestExpiredWait);
      EXPECT_NE(sacn::Receiver::GetExpiredWait(), kTestExpiredWait);
    }
  }

  void VerifyMergeReceiverInit(sacn_features_t features)
  {
    sacn::MergeReceiver                      merge_receiver;
    NiceMock<MockMergeReceiverNotifyHandler> notify;

    sacn::MergeReceiver::Settings settings;
    settings.universe_id = kTestUniverse;

    if ((features & kAllOtherFeatures) == kAllOtherFeatures)
    {
      EXPECT_TRUE(merge_receiver.Startup(settings, notify));

      EXPECT_EQ(merge_receiver.GetUniverse(), kTestUniverse);
      EXPECT_TRUE(merge_receiver.ChangeUniverse(kTestUniverse + 1u));

      EXPECT_TRUE(merge_receiver.ResetNetworking());

      merge_receiver.Shutdown();
    }
    else
    {
      EXPECT_EQ(merge_receiver.Startup(settings, notify), kEtcPalErrNotInit);

      EXPECT_EQ(merge_receiver.GetUniverse(), kEtcPalErrNotInit);
      EXPECT_EQ(merge_receiver.ChangeUniverse(kTestUniverse + 1u), kEtcPalErrNotInit);

      EXPECT_EQ(merge_receiver.ResetNetworking(), kEtcPalErrNotInit);
    }
  }

  void VerifySourceInit(sacn_features_t features)
  {
    sacn::Source source;

    sacn::Source::Settings settings;
    settings.cid  = etcpal::Uuid::V4();
    settings.name = kTestName;

    sacn::Source::UniverseSettings universe_settings;
    universe_settings.universe = kTestUniverse;

    if ((features & kAllOtherFeatures) == kAllOtherFeatures)
    {
      EXPECT_TRUE(source.Startup(settings));

      EXPECT_TRUE(source.AddUniverse(universe_settings));
      EXPECT_TRUE(source.AddUnicastDestination(kTestUniverse, kTestAddr));

      EXPECT_TRUE(source.ChangeName(std::string(kTestName) + " 2"));
      EXPECT_TRUE(source.ChangePriority(kTestUniverse, kTestPriority));
      EXPECT_TRUE(source.ChangePreviewFlag(kTestUniverse, true));

      EXPECT_TRUE(source.SendNow(kTestUniverse, kTestStartCode, kTestValues.data(), kTestValues.size()));

      EXPECT_TRUE(source.ResetNetworking());
    }
    else
    {
      EXPECT_EQ(source.Startup(settings), kEtcPalErrNotInit);

      EXPECT_EQ(source.AddUniverse(universe_settings), kEtcPalErrNotInit);
      EXPECT_EQ(source.ChangeName(std::string(kTestName) + " 2"), kEtcPalErrNotInit);

      EXPECT_EQ(source.ResetNetworking(), kEtcPalErrNotInit);
    }
  }

  void VerifySourceDetectorInit(sacn_features_t features)
  {
    NiceMock<MockSourceDetectorNotifyHandler> notify;

    if ((features & kAllOtherFeatures) == kAllOtherFeatures)
    {
      EXPECT_TRUE(sacn::SourceDetector::Startup(notify));
      EXPECT_TRUE(sacn::SourceDetector::ResetNetworking());
    }
    else
    {
      EXPECT_EQ(sacn::SourceDetector::Startup(notify), kEtcPalErrNotInit);
      EXPECT_EQ(sacn::SourceDetector::ResetNetworking(), kEtcPalErrNotInit);
    }
  }
};

TEST_F(InitTest, HandlesNothingInitialized)
{
  VerifyInit(kNoFeatures);
}

TEST_F(InitTest, InitializesDmxMergerFeature)
{
  EXPECT_TRUE(sacn::Init(SACN_FEATURE_DMX_MERGER));
  VerifyInit(SACN_FEATURE_DMX_MERGER);

  sacn::Deinit(SACN_FEATURE_DMX_MERGER);
  VerifyInit(kNoFeatures);
}

TEST_F(InitTest, InitializesAllFeaturesSeparately)
{
  EXPECT_TRUE(sacn::Init(SACN_FEATURE_DMX_MERGER));
  VerifyInit(SACN_FEATURE_DMX_MERGER);
  EXPECT_TRUE(sacn::Init());
  VerifyInit(SACN_FEATURES_ALL);

  sacn::Deinit();
  VerifyInit(SACN_FEATURE_DMX_MERGER);
  sacn::Deinit(SACN_FEATURE_DMX_MERGER);
  VerifyInit(kNoFeatures);
}

TEST_F(InitTest, InitializesAllFeaturesAtOnce)
{
  EXPECT_TRUE(sacn::Init());
  VerifyInit(SACN_FEATURES_ALL);

  sacn::Deinit();
  VerifyInit(kNoFeatures);
}
