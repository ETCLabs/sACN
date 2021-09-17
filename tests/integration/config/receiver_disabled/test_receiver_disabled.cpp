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

#include "sacn/receiver.h"

#include <limits>
#include "etcpal/cpp/error.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/netint.h"
#include "sacn/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/receiver.h"
#include "gtest/gtest.h"
#include "fff.h"

class TestReceiverDisabled : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();

    etcpal_netint_get_num_interfaces_fake.return_val = 1;
    etcpal_netint_get_interfaces_fake.return_val = &fake_netint_;

    ASSERT_EQ(sacn_init(nullptr, nullptr), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_deinit();
  }
  
  EtcPalNetintInfo fake_netint_;
};

#if SACN_DYNAMIC_MEM
TEST_F(TestReceiverDisabled, ReceiverIsEnabledInDynamicMode)
{
  // Run the API functions to confirm everything links.
  sacn_receiver_create(nullptr, nullptr, nullptr);
  sacn_receiver_destroy(SACN_RECEIVER_INVALID);
  sacn_receiver_get_universe(SACN_RECEIVER_INVALID, nullptr);
  sacn_receiver_change_universe(SACN_RECEIVER_INVALID, 0);
  sacn_receiver_reset_networking(nullptr);
  sacn_receiver_reset_networking_per_receiver(nullptr, nullptr, 0);

  sacn_receiver_set_expired_wait(123u);
  EXPECT_EQ(sacn_receiver_get_expired_wait(), 123u);
}
#endif  // SACN_DYNAMIC_MEM
