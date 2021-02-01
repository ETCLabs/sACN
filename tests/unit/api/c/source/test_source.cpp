/******************************************************************************
 * Copyright 2020 ETC Inc.
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

#include "sacn/source.h"

#include <limits>
#include "etcpal/cpp/error.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/sockets.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/source.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestSource TestSourceDynamic
#else
#define TestSource TestSourceStatic
#endif

static const etcpal::Uuid kTestLocalCid = etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22");
static const std::string kTestLocalName = std::string("Test Source");
static const etcpal::SockAddr kTestRemoteAddrV4(etcpal::IpAddr::FromString("10.101.1.1"), 8888);
static const etcpal::SockAddr kTestRemoteAddrV6(etcpal::IpAddr::FromString("2001:db8::1234:5678"), 8888);

class TestSource : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_sockets_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_source_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_source_deinit();
    sacn_mem_deinit();
  }

  etcpal::Expected<sacn_source_t> AddSource()
  {
    SacnSourceConfig config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
    config.cid = kTestLocalCid.get();
    config.name = kTestLocalName.c_str();

    sacn_source_t handle;
    etcpal_error_t err = sacn_source_create(&config, &handle);

    if (err == kEtcPalErrOk)
      return handle;
    else
      return err;
  }

  etcpal::Expected<uint16_t> AddUniverse(sacn_source_t source)
  {
    SacnSourceUniverseConfig config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;

    SacnSource* source_state;
    etcpal_error_t error = lookup_source(source, &source_state);

    if (error == kEtcPalErrOk)
    {
      config.universe = static_cast<uint16_t>((source_state->num_universes + 1));
      error = sacn_source_add_universe(source, &config, NULL, 0);
    }

    if (error == kEtcPalErrOk)
      return config.universe;
    else
      return error;
  }

  etcpal::Expected<etcpal::IpAddr> AddUnicastDestination(sacn_source_t source, uint16_t universe)
  {
    EtcPalIpAddr test_ip = kTestRemoteAddrV4.ip().get();

    SacnSource* source_state;
    SacnSourceUniverse* universe_state;
    etcpal_error_t error = lookup_source_and_universe(source, universe, &source_state, &universe_state);

    if (error == kEtcPalErrOk)
    {
      test_ip.addr.v4 += universe_state->num_unicast_dests;
      error = sacn_source_add_unicast_destination(source, universe, &test_ip);
    }

    if (error == kEtcPalErrOk)
      return etcpal::IpAddr(test_ip);
    else
      return error;
  }
};

TEST_F(TestSource, DeinitTriggersTerminate)
{
  sacn_source_t source = AddSource().value_or(SACN_SOURCE_INVALID);
  uint16_t universe = AddUniverse(source).value_or(0);
}
