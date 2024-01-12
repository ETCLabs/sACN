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

#include "sacn/merge_receiver.h"

#include <limits>
#include "etcpal/cpp/error.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/netint.h"
#include "etcpal_mock/socket.h"
#include "sacn/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/merge_receiver.h"
#include "gtest/gtest.h"
#include "fff.h"

class TestMergeReceiverDisabled : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();

    etcpal_netint_get_interfaces_fake.custom_fake = [](EtcPalNetintInfo* netints, size_t* num_netints) {
      if (netints)
        netints[0] = fake_netint_;

      *num_netints = 1u;

      return kEtcPalErrOk;
    };

    etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* id) {
      *id = static_cast<etcpal_socket_t>(0);
      return kEtcPalErrOk;
    };

    fake_netint_.addr.type = kEtcPalIpTypeV4;

    ASSERT_EQ(sacn_init(nullptr, nullptr), kEtcPalErrOk);
  }

  void TearDown() override { sacn_deinit(); }

  static EtcPalNetintInfo fake_netint_;
};

EtcPalNetintInfo TestMergeReceiverDisabled::fake_netint_;

#if SACN_DYNAMIC_MEM
TEST_F(TestMergeReceiverDisabled, MergeReceiverIsEnabledInDynamicMode)
{
  // Run the API functions to confirm everything links.
  sacn_merge_receiver_create(nullptr, nullptr, nullptr);
  sacn_merge_receiver_destroy(SACN_MERGE_RECEIVER_INVALID);
  sacn_merge_receiver_get_universe(SACN_MERGE_RECEIVER_INVALID, nullptr);
  sacn_merge_receiver_change_universe(SACN_MERGE_RECEIVER_INVALID, 0);
  sacn_merge_receiver_reset_networking(nullptr);
  sacn_merge_receiver_reset_networking_per_receiver(nullptr, nullptr, 0);
}
#endif  // SACN_DYNAMIC_MEM
