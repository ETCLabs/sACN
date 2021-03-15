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

#include "sacn/private/receiver_state.h"

#include <limits>
#include <optional>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/thread.h"
#include "etcpal_mock/timer.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/sockets.h"
#include "sacn_mock/private/source_loss.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/pdu.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestReceiverPapDisabled TestReceiverPapDisabledDynamic
#else
#define TestReceiverPapDisabled TestReceiverPapDisabledStatic
#endif

FAKE_VOID_FUNC(universe_data, sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*, bool,
               void*);
FAKE_VOID_FUNC(sources_lost, sacn_receiver_t, uint16_t, const SacnLostSource*, size_t, void*);
FAKE_VOID_FUNC(sampling_period_started, sacn_receiver_t, uint16_t, void*);
FAKE_VOID_FUNC(sampling_period_ended, sacn_receiver_t, uint16_t, void*);
FAKE_VOID_FUNC(source_pap_lost, sacn_receiver_t, uint16_t, const SacnRemoteSource*, void*);
FAKE_VOID_FUNC(source_limit_exceeded, sacn_receiver_t, uint16_t, void*);

class TestReceiverPapDisabled : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_sockets_reset_all_fakes();
    sacn_source_loss_reset_all_fakes();
    
    RESET_FAKE(universe_data);
    RESET_FAKE(sources_lost);
    RESET_FAKE(sampling_period_started);
    RESET_FAKE(sampling_period_ended);
    RESET_FAKE(source_pap_lost);
    RESET_FAKE(source_limit_exceeded);

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_receiver_state_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_receiver_state_deinit();
    sacn_mem_deinit();
  }
};

TEST_F(TestReceiverPapDisabled, Foo)
{
  // TODO
}
