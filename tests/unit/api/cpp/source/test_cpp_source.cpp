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

#include "sacn/cpp/common.h"
#include "sacn/cpp/source.h"

#include <limits>
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/source_loss.h"
#include "sacn_mock/private/sockets.h"
#include "sacn_mock/private/source.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "gtest/gtest.h"

#if SACN_DYNAMIC_MEM
#define TestSource TestCppSourceDynamic
#else
#define TestSource TestCppSourceStatic
#endif

static const etcpal::Uuid kTestLocalCid = etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22");
static const std::string kTestLocalName = "Test Source";

class TestSource : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_source_loss_reset_all_fakes();
    sacn_sockets_reset_all_fakes();
    sacn_source_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
  }

  void TearDown() override { sacn_mem_deinit(); }
};

TEST_F(TestSource, SettingsConstructorWorks)
{
  sacn::Source::Settings settings(kTestLocalCid, kTestLocalName);
  EXPECT_EQ(ETCPAL_UUID_CMP(&settings.cid.get(), &kTestLocalCid.get()), 0);
  EXPECT_EQ(settings.name, kTestLocalName);
  EXPECT_EQ(settings.universe_count_max, static_cast<size_t>(SACN_SOURCE_INFINITE_UNIVERSES));
  EXPECT_EQ(settings.manually_process_source, false);
  EXPECT_EQ(settings.ip_supported, kSacnIpV4AndIpV6);
  EXPECT_EQ(settings.keep_alive_interval, SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT);
}

TEST_F(TestSource, SettingsIsValidWorks)
{
  sacn::Source::Settings valid_settings(kTestLocalCid, kTestLocalName);
  sacn::Source::Settings invalid_settings(etcpal::Uuid(), kTestLocalName);

  EXPECT_EQ(valid_settings.IsValid(), true);
  EXPECT_EQ(invalid_settings.IsValid(), false);
}
