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

#include "sacn/cpp/source_detector.h"

#include <limits>
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/source_loss.h"
#include "sacn_mock/private/sockets.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/source_detector.h"
#include "gtest/gtest.h"

#if SACN_DYNAMIC_MEM
#define TestSourceDetector TestCppSourceDetectorDynamic
#else
#define TestSourceDetector TestCppSourceDetectorStatic
#endif

class TestSourceDetector : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_source_loss_reset_all_fakes();
    sacn_sockets_reset_all_fakes();

    ASSERT_EQ(sacn_receiver_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_source_detector_mem_init(), kEtcPalErrOk);
    ASSERT_EQ(sacn_source_detector_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_source_detector_deinit();
    sacn_source_detector_mem_deinit();
    sacn_receiver_mem_deinit();
  }
};

TEST_F(TestSourceDetector, TestFoo)
{
  // TODO: CLEAN UP TESTING
}
